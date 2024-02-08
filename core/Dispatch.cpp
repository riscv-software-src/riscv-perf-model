// <Dispatch.cpp> -*- C++ -*-

#include <algorithm>
#include "CoreUtils.hpp"
#include "Dispatch.hpp"
#include "sparta/events/StartupEvent.hpp"

namespace olympia
{
    const char Dispatch::name[] = "dispatch";

    // Constructor
    Dispatch::Dispatch(sparta::TreeNode* node, const DispatchParameterSet* p) :
        sparta::Unit(node),
        dispatch_queue_("dispatch_queue", p->dispatch_queue_depth, node->getClock(),
                        getStatisticSet()),
        num_to_dispatch_(p->num_to_dispatch),
        dispatch_queue_depth_(p->dispatch_queue_depth)
    {
        weighted_unit_distribution_context_.assignContextWeights(p->context_weights);
        dispatch_queue_.enableCollection(node);

        // Start the no instructions counter
        stall_counters_[current_stall_].startCounting();

        // Register consuming events with the InPorts.
        in_dispatch_queue_write_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, dispatchQueueAppended_, InstGroupPtr));
        /*
            We're creating a pipe mapping for each issue queue types:
            pipe_map_ = {"int": ["issue_queue"]}
            so we have to find for each pipe, what issue queue is mapped to this, which is a search
           over pipe_topology and issue_queue_topology For example if we have the topology defined
           for alus as: pipe_topology_alu_pipes:
                [["int"], # alu0
                ["int", "div"], # alu1
                ["int", "mul"], # alu2
                ["int", "mul", "i2f", "cmov"], # alu3
                ["int"], # alu4
                ["int"] # alu5]
            and an issue queue topology of:
            issue_queue_topology:
                [["alu0"], # issue queue 0
                ["alu1", "alu2"], # issue queue 1
                ["alu3", "alu4", "alu5"], # issue queue 2
                ["fpu0", "fpu1"],
                ["br0", "br1"]]
            The algorithm below loops over the issue_queue_topology, finds the associated pipe
           topology for it, then looks up the associated issue queue dispatcher to associate with
           that pipe topology.

            Example flow:
                Iterate onto alu0, look at pipe_toplology_alu_pipes[0] which is "int",
                set in pipe_map_ "int": issue_queue0 dispatcher
        */

        const auto pipelines = olympia::coreutils::getPipeTopology(node->getParent(), "pipelines");

        const auto issue_queue_to_pipe_map =
            olympia::coreutils::getPipeTopology(node->getParent(), "issue_queue_to_pipe_map");
        const auto issue_queue_rename =
            olympia::coreutils::getPipeTopology(node->getParent(), "issue_queue_rename");
        for (size_t iq_num = 0; iq_num < issue_queue_to_pipe_map.size(); ++iq_num)
        {
            // set port, create dispatcher for issue queue
            const auto iq = issue_queue_to_pipe_map[iq_num];
            auto iq_name = "iq" + std::to_string(iq_num);
            if (issue_queue_rename.size() > 0)
            {
                sparta_assert(issue_queue_rename[iq_num][0] == iq_name,
                              "Rename mapping for issue queue is not in order or the original unit "
                              "name is not equal to the unit name, check spelling!") iq_name =
                    issue_queue_rename[iq_num][1];
            }
            auto & in_credit_port = in_credit_ports_.emplace_back(
                new sparta::DataInPort<uint32_t>(&unit_port_set_, "in_" + iq_name + "_credits"));
            in_credit_port->enableCollection(node);
            auto & out_inst_port =
                out_inst_ports_.emplace_back(new sparta::DataOutPort<InstQueue::value_type>(
                    &unit_port_set_, "out_" + iq_name + "_write"));
            std::shared_ptr<Dispatcher> dispatcher = std::make_shared<Dispatcher>(
                iq_name, this, info_logger_, in_credit_port.get(), out_inst_port.get());

            // store in dispatchers_ which issue queues map to which pipe target
            const auto pipe_target_start = std::stoi(iq[0]);
            // check if we have a 1 to 1 mapping
            // i.e ["0"] would just mean this issue queue maps to exe0
            // i.e ["0", "1"] would mean this issue queue maps to exe0 and exe1
            auto pipe_target_end = pipe_target_start;
            if (iq.size() > 1)
            {
                pipe_target_end = std::stoi(iq[1]);
            }
            pipe_target_end++;
            for (int pipe_idx = pipe_target_start; pipe_idx < pipe_target_end; ++pipe_idx)
            {
                for (auto pipe_target : pipelines[pipe_idx])
                {
                    const auto tgt_pipe = InstArchInfo::execution_pipe_map.find(pipe_target);
                    const auto iq_vector = dispatchers_[static_cast<size_t>(tgt_pipe->second)];
                    // check if this issue queue already exists in pipe to issue queue
                    // mapping, because multiple execution units in the same issue queue can
                    // have the same pipe target, we don't want duplicate entries i.e iq0
                    // has "exe0", "exe1", which both support "INT", we only want one
                    // definition of iq0 in the mapping
                    if (std::find(iq_vector.begin(), iq_vector.end(), dispatcher)
                        == iq_vector.end())
                    {
                        ILOG("mapping target: " << tgt_pipe->second << dispatcher->getName())
                        dispatchers_[static_cast<size_t>(tgt_pipe->second)].emplace_back(
                            dispatcher);
                    }
                }
            }
        }

        // Special case for the LSU
        dispatchers_[static_cast<size_t>(InstArchInfo::TargetPipe::LSU)].emplace_back(
            new Dispatcher("lsu", this, info_logger_, &in_lsu_credits_, &out_lsu_write_));
        in_lsu_credits_.enableCollection(node);

        in_reorder_credits_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, robCredits_, uint32_t));
        in_reorder_credits_.enableCollection(node);

        in_reorder_flush_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(
            Dispatch, handleFlush_, FlushManager::FlushingCriteria));
        in_reorder_flush_.enableCollection(node);

        blocking_dispatcher_ = InstArchInfo::TargetPipe::INT;
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Dispatch, sendInitialCredits_));
    }

    void Dispatch::scheduleDispatchSession()
    {
        if (credits_rob_ > 0 && (dispatch_queue_.size() > 0))
        {
            // See if the one of the original blocking dispatcher
            // types is still blocking.
            if (blocking_dispatcher_ != InstArchInfo::TargetPipe::UNKNOWN)
            {
                auto & dispatchers = dispatchers_[blocking_dispatcher_];
                for (auto & disp : dispatchers)
                {
                    if (disp->canAccept())
                    {
                        blocking_dispatcher_ = InstArchInfo::TargetPipe::UNKNOWN;
                        ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
                        break;
                    }
                    else
                    {
                        ILOG("dispatcher '" << disp->getName() << "' still cannot accept an inst");
                    }
                }
            }
            else
            {
                ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
            }
        }
        else
        {
            ILOG("no rob credits or no instructions to process");
        }
    }

    void Dispatch::sendInitialCredits_()
    {
        out_dispatch_queue_credits_.send(dispatch_queue_.capacity());
    }

    void Dispatch::robCredits_(const uint32_t &)
    {
        uint32_t nc = in_reorder_credits_.pullData();
        credits_rob_ += nc;
        scheduleDispatchSession();
        ILOG("ROB got " << nc << " credits, total: " << credits_rob_);
    }

    void Dispatch::dispatchQueueAppended_(const InstGroupPtr & inst_grp)
    {
        ILOG("queue appended: " << inst_grp);
        for (auto & i : *in_dispatch_queue_write_.pullData())
        {
            sparta_assert(dispatch_queue_.size() < dispatch_queue_depth_,
                          "pushing more instructions into Dispatch Queue than the limit: "
                              << dispatch_queue_depth_);
            dispatch_queue_.push(i);
        }
        scheduleDispatchSession();
    }

    void Dispatch::dispatchInstructions_()
    {
        uint32_t num_dispatch = std::min(dispatch_queue_.size(), num_to_dispatch_);
        num_dispatch = std::min(credits_rob_, num_dispatch);

        ILOG("Num to dispatch: " << num_dispatch);

        // Stop the current counter
        stall_counters_[current_stall_].stopCounting();

        if (num_dispatch == 0)
        {
            stall_counters_[current_stall_].startCounting();
            return;
        }

        current_stall_ = NOT_STALLED;

        InstGroupPtr insts_dispatched =
            sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);
        ;
        bool keep_dispatching = true;
        for (uint32_t i = 0; (i < num_dispatch) && keep_dispatching; ++i)
        {
            bool dispatched = false;
            InstPtr & ex_inst_ptr = dispatch_queue_.access(0);
            Inst & ex_inst = *ex_inst_ptr;
            const auto target_pipe = ex_inst.getPipe();

            sparta_assert(target_pipe != InstArchInfo::TargetPipe::UNKNOWN,
                          "Have an instruction that doesn't know where to go: " << ex_inst);

            if (SPARTA_EXPECT_FALSE(target_pipe == InstArchInfo::TargetPipe::SYS))
            {
                ex_inst.setStatus(Inst::Status::COMPLETED);
                // Indicate that this instruction was dispatched
                // -- it goes right to the ROB
                dispatched = true;
            }
            else
            {
                // Get the dispatchers used to dispatch the target.
                // Find a ready-to-go dispatcher
                sparta_assert(static_cast<size_t>(target_pipe) < InstArchInfo::N_TARGET_PIPES,
                              "Target pipe: " << target_pipe
                                              << " not found in dispatchers, make sure pipe is "
                                                 "defined and has an assigned dispatcher");
                auto & dispatchers = dispatchers_[static_cast<size_t>(target_pipe)];
                sparta_assert(dispatchers.size() > 0,
                              "Pipe Target: "
                                  << target_pipe
                                  << " doesn't have any execution unit that can handle that target "
                                     "pipe. Did you define it in the yaml properly?");
                // so we have a map here that checks for which valid dispatchers for that
                // instruction target pipe map needs to be: "int": [exe0, exe1, exe2]
                if (target_pipe != InstArchInfo::TargetPipe::LSU)
                {
                    uint32_t max_credits = 0;
                    olympia::Dispatcher* best_dispatcher = nullptr;
                    // find the dispatcher with the most amount of credits, i.e the issue queue with
                    // the least amount of entries
                    for (auto & dispatcher_iq : dispatchers)
                    {
                        if (dispatcher_iq->canAccept() && dispatcher_iq->getCredits() > max_credits)
                        {
                            best_dispatcher = dispatcher_iq.get();
                            max_credits = dispatcher_iq->getCredits();
                        }
                    }
                    if (best_dispatcher != nullptr)
                    {
                        best_dispatcher->acceptInst(ex_inst_ptr);
                        ++unit_distribution_[target_pipe];
                        ++unit_distribution_context_.context(target_pipe);
                        ++weighted_unit_distribution_context_.context(target_pipe);

                        ex_inst_ptr->setStatus(Inst::Status::DISPATCHED);
                        ILOG("Sending instruction: " << ex_inst_ptr << " to "
                                                     << best_dispatcher->getName()
                                                     << " of target type: " << target_pipe);
                        dispatched = true;
                    }
                }
                else
                {
                    for (auto & disp : dispatchers)
                    {
                        if (disp->canAccept())
                        {
                            disp->acceptInst(ex_inst_ptr);
                            ++unit_distribution_[target_pipe];
                            ++(unit_distribution_context_.context(target_pipe));
                            ++(weighted_unit_distribution_context_.context(target_pipe));

                            ex_inst_ptr->setStatus(Inst::Status::DISPATCHED);
                            ILOG("Sending instruction: " << ex_inst_ptr << " to "
                                                         << disp->getName());
                            dispatched = true;

                            break;
                        }
                        else
                        {
                            ILOG(disp->getName() << " cannot accept inst: " << ex_inst_ptr);
                            blocking_dispatcher_ = target_pipe;
                        }
                    }
                }
                if (false == dispatched)
                {
                    current_stall_ = static_cast<StallReason>(target_pipe);
                    keep_dispatching = false;
                }
            }

            if (dispatched)
            {
                insts_dispatched->emplace_back(ex_inst_ptr);
                dispatch_queue_.pop();
                --credits_rob_;
            }
            else
            {
                ILOG("Could not dispatch: " << ex_inst_ptr << " stall: " << current_stall_);
                break;
            }
        }
        for (auto & dispatchers : dispatchers_)
        {
            for (auto & disp : dispatchers)
            {
                disp->reset();
            }
        }

        if (!insts_dispatched->empty())
        {
            out_dispatch_queue_credits_.send(static_cast<uint32_t>(insts_dispatched->size()));
            out_reorder_write_.send(insts_dispatched);
        }

        if ((credits_rob_ > 0) && (dispatch_queue_.size() > 0) && (current_stall_ == NOT_STALLED))
        {
            ev_dispatch_insts_.schedule(1);
        }

        stall_counters_[current_stall_].startCounting();
    }

    void Dispatch::handleFlush_(const FlushManager::FlushingCriteria & criteria)
    {
        ILOG("Got a flush call for " << criteria);
        out_dispatch_queue_credits_.send(dispatch_queue_.size());
        dispatch_queue_.clear();
        out_reorder_write_.cancel();
    }
} // namespace olympia
