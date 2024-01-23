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

        auto execution_topology = coreutils::getExecutionTopology(node->getParent());

        auto issue_queue_pipe_topology =
            olympia::coreutils::getPipeTopology(node->getParent(), "issue_queue_topology");
        // creating issue queue units by walking the issue queue pipe topology param
        for (size_t i = 0; i < issue_queue_pipe_topology.size(); i++)
        {
            const auto exe_units = issue_queue_pipe_topology[i];
            const auto exe_unit_first = exe_units[0];
            // for each type of execution unit i.e alu, fpu, br
            // create an issue queue by looking at the first element in the issue queue definition
            // i.e ["alu0", "alu1"] would mean that this issue queue is an alu iq, because we look
            // at alu0 and find the corresponding execution unit type
            for (const auto opt : core_types::issue_queue_types)
            {
                size_t found = exe_unit_first.find(opt);
                if (found != std::string::npos)
                {
                    const std::string iq_name = "iq" + std::to_string(i);
                    const auto target_itr = InstArchInfo::dispatch_target_map.find(opt);
                    sparta_assert(target_itr != InstArchInfo::dispatch_target_map.end(),
                                  "Execution unit not found in dispatch_target_map, did you define "
                                  "the execution unit type?") auto & in_credit_port =
                        in_credit_ports_.emplace_back(new sparta::DataInPort<uint32_t>(
                            &unit_port_set_, "in_" + iq_name + "_credits"));
                    in_credit_port->enableCollection(node);

                    auto & out_inst_port =
                        out_inst_ports_.emplace_back(new sparta::DataOutPort<InstQueue::value_type>(
                            &unit_port_set_, "out_" + iq_name + "_write"));
                    dispatchers_[target_itr->second].emplace_back(new Dispatcher(
                        iq_name, this, info_logger_, in_credit_port.get(), out_inst_port.get()));
                }
            }
        }

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
        auto setup_pipe_map = [this, issue_queue_pipe_topology, node](const std::string & exe_unit)
        {
            auto exe_unit_pipe_topology = olympia::coreutils::getPipeTopology(
                node->getParent(), "pipe_topology_" + exe_unit + "_pipes");
            std::unordered_map<std::string, std::vector<std::string>> exe_unit_to_pipe_mapping;
            // Get mapping of execution units
            for (size_t i = 0; i < exe_unit_pipe_topology.size(); ++i)
            {
                std::string unit_name = exe_unit + std::to_string(i);
                exe_unit_to_pipe_mapping[unit_name] = exe_unit_pipe_topology[i];
            }
            std::unordered_map<std::string, Dispatcher*> string_to_dispatcher;
            const auto target_itr = InstArchInfo::dispatch_target_map.find(exe_unit);
            sparta_assert(
                target_itr != InstArchInfo::dispatch_target_map.end(),
                "Could not find execution unit: "
                    << exe_unit
                    << " in dispatch_target_map, mispelled unit name?") auto & dispatchers =
                dispatchers_[target_itr->second];
            for (auto & dispatcher : dispatchers)
            {
                if (!string_to_dispatcher.empty())
                {
                    sparta_assert(string_to_dispatcher.find(dispatcher->getName())
                                      == string_to_dispatcher.end(),
                                  "string_to_dispatcher has two dispatchers with the same name: "
                                      << dispatcher->getName())
                }
                string_to_dispatcher[dispatcher->getName()] = dispatcher.get();
            }
            // setting up pipe to issue queue mapping
            // "INT": ["iq0", "iq1"]
            for (size_t j = 0; j < issue_queue_pipe_topology.size(); ++j)
            {
                // loop through each issue queue definition i.e ["alu0", "alu1"]
                for (size_t k = 0; k < issue_queue_pipe_topology[j].size(); ++k)
                {
                    // loop through each element in an issue queue, i.e "alu0", "alu1"
                    const std::string tgt_unit_name = issue_queue_pipe_topology[j][k];
                    if (tgt_unit_name.find(exe_unit) != std::string::npos)
                    {
                        // find the corresponding pipes for the target unit, i.e "alu0" can take
                        // pipe targets of "INT", "MUL"
                        const std::vector<std::string> & pipes = exe_unit_to_pipe_mapping[tgt_unit_name];
                        // for each pipe target, insert into a map that defines a pipe to issue
                        // queue target: "INT": ["iq0", "iq1"] "MUL": ["iq2"]
                        for (auto pipe : pipes)
                        {
                            const auto tgt_pipe = InstArchInfo::execution_pipe_map.find(pipe);
                            const auto iq_vector = pipe_to_dispatcher_map_[tgt_pipe->second];
                            // check if this issue queue alreay exists in pipe to issue queue
                            // mapping, because multiple execution units in the same issue queue can
                            // have the same pipe target, we don't want duplicate entries i.e iq0
                            // has "alu0", "alu1", which both support "INT", we only want one
                            // definition of iq0 in the mapping
                            if (std::find(iq_vector.begin(), iq_vector.end(),
                                          string_to_dispatcher["iq" + std::to_string(j)])
                                == iq_vector.end())
                            {
                                pipe_to_dispatcher_map_[tgt_pipe->second].emplace_back(
                                    string_to_dispatcher["iq" + std::to_string(j)]);
                            }
                        }
                    }
                }
            }
        };

        for (auto iq_type : core_types::issue_queue_types)
        {
            setup_pipe_map(iq_type);
        }

        // Special case for the LSU
        dispatchers_[InstArchInfo::TargetUnit::LSU].emplace_back(
            new Dispatcher("lsu", this, info_logger_, &in_lsu_credits_, &out_lsu_write_));
        in_lsu_credits_.enableCollection(node);

        in_reorder_credits_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, robCredits_, uint32_t));
        in_reorder_credits_.enableCollection(node);

        in_reorder_flush_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(
            Dispatch, handleFlush_, FlushManager::FlushingCriteria));
        in_reorder_flush_.enableCollection(node);

        blocking_dispatcher_ = InstArchInfo::TargetUnit::ALU;
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Dispatch, sendInitialCredits_));
    }

    void Dispatch::scheduleDispatchSession()
    {
        if (credits_rob_ > 0 && (dispatch_queue_.size() > 0))
        {
            // See if the one of the original blocking dispatcher
            // types is still blocking.
            if (blocking_dispatcher_ != InstArchInfo::TargetUnit::NONE)
            {
                auto & dispatchers = dispatchers_[blocking_dispatcher_];
                for (auto & disp : dispatchers)
                {
                    if (disp->canAccept())
                    {
                        blocking_dispatcher_ = InstArchInfo::TargetUnit::NONE;
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
            const auto target_unit = ex_inst.getUnit();

            sparta_assert(target_unit != InstArchInfo::TargetUnit::UNKNOWN,
                          "Have an instruction that doesn't know where to go: " << ex_inst);

            if (SPARTA_EXPECT_FALSE(target_unit == InstArchInfo::TargetUnit::ROB))
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
                auto & dispatchers = dispatchers_[target_unit];
                // so we have a map here that checks for which valid dispatchers for that
                // instruction target pipe map needs to be: "int": [alu0, alu1, alu2, alu3] "mul":
                // [alu0, alu1] disp->getName() ex_inst_ptr->getTarget()
                if (ex_inst_ptr->getUnit() != InstArchInfo::TargetUnit::LSU)
                {
                    const auto pipe = ex_inst_ptr->getPipe();
                    const auto it = pipe_to_dispatcher_map_.find(pipe); // rename to dispatcher_map_
                    // turn into assert
                    sparta_assert(it != pipe_to_dispatcher_map_.end(),
                                  "No dispatcher found for pipe target: "
                                      << pipe
                                      << " check if pipe to dispatcher mapping is correct!");
                    auto dispatchers_iq = it->second;
                    uint32_t max_credits = 0;
                    olympia::Dispatcher* best_dispatcher; // make a raw dispatcher pointer
                    // find the dispatcher with the most amount of credits, i.e the issue queue with
                    // the least amount of entries
                    for (auto & dispatcher_iq : dispatchers_iq)
                    {
                        if (dispatcher_iq->canAccept() && dispatcher_iq->getCredits() > max_credits)
                        {
                            best_dispatcher = dispatcher_iq;
                            max_credits = dispatcher_iq->getCredits();
                        }
                    }
                    if (max_credits != 0)
                    {
                        best_dispatcher->acceptInst(ex_inst_ptr);
                        ++unit_distribution_[target_unit];
                        ++unit_distribution_context_.context(target_unit);
                        ++weighted_unit_distribution_context_.context(target_unit);

                        ex_inst_ptr->setStatus(Inst::Status::DISPATCHED);
                        ILOG("Sending instruction: " << ex_inst_ptr << " to "
                                                     << best_dispatcher->getName());
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
                            ++unit_distribution_[target_unit];
                            ++(unit_distribution_context_.context(target_unit));
                            ++(weighted_unit_distribution_context_.context(target_unit));

                            ex_inst_ptr->setStatus(Inst::Status::DISPATCHED);
                            ILOG("Sending instruction: " << ex_inst_ptr << " to "
                                                         << disp->getName());
                            dispatched = true;

                            break;
                        }
                        else
                        {
                            ILOG(disp->getName() << " cannot accept inst: " << ex_inst_ptr);
                            blocking_dispatcher_ = target_unit;
                        }
                    }
                }
                if (false == dispatched)
                {
                    current_stall_ = static_cast<StallReason>(target_unit);
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
