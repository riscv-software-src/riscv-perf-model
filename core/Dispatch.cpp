// <Dispatch.cpp> -*- C++ -*-

#include <algorithm>
#include "CoreUtils.hpp"
#include "Dispatch.hpp"

#include "sparta/events/StartupEvent.hpp"

namespace olympia
{
    const char Dispatch::name[] = "dispatch";

    // Constructor
    Dispatch::Dispatch(sparta::TreeNode * node,
                       const DispatchParameterSet * p) :
        sparta::Unit(node),
        dispatch_queue_("dispatch_queue", p->dispatch_queue_depth,
                        node->getClock(), getStatisticSet()),
        num_to_dispatch_(p->num_to_dispatch)
    {
        weighted_unit_distribution_context_.assignContextWeights(p->context_weights);
        dispatch_queue_.enableCollection(node);

        // Start the no instructions counter
        stall_counters_[current_stall_].startCounting();

        // Register consuming events with the InPorts.
        in_dispatch_queue_write_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, dispatchQueueAppended_,
                                                                    InstGroupPtr));

        auto execution_topology = coreutils::getExecutionTopology(node->getParent());


        // currently is:
        // alu 6, br 2, fpu 2
        // alu_dispatch -> 6 alus
        // br_dispatch -> 2 brs
        // fpu_dispatch -> 2 fpus
        // Create Disptchers for ALU, FPU, BR -- one to many of them
        // depending on the execution_topology extension

        /*
        dispatcher["alu"]
        */
        // for (auto exe_unit_pair : execution_topology)
        // {
        //     const auto tgt_name   = exe_unit_pair[0];
        //     const auto unit_count = exe_unit_pair[1];

        //     const auto exe_idx = (unsigned int) std::stoul(unit_count);
        //     sparta_assert(exe_idx > 0, "Expected more than 0 units! " << tgt_name);
        //     for(uint32_t unit_num = 0; unit_num < exe_idx; ++unit_num)
        //     {
        //         const std::string unit_name = tgt_name + std::to_string(unit_num);

        //         // Create an InPort and an OutPort for credits and
        //         // instruction send
        //         auto & in_credit_port = in_credit_ports_.emplace_back
        //             (new sparta::DataInPort<uint32_t>(&unit_port_set_, "in_"+unit_name+"_credits"));
        //         in_credit_port->enableCollection(node);

        //         auto & out_inst_port = out_inst_ports_.emplace_back
        //             (new sparta::DataOutPort<InstQueue::value_type>(&unit_port_set_, "out_"+unit_name+"_write"));

        //         // Create a Dispatcher for this target type
        //         const auto target_itr = InstArchInfo::dispatch_target_map.find(tgt_name);
        //         sparta_assert(target_itr != InstArchInfo::dispatch_target_map.end(),
        //                       "Unknown target unit: " << tgt_name << " when parsing the execution_topology extension");
        //         dispatchers_[target_itr->second].emplace_back(new Dispatcher(unit_name,
        //                                                                      this,
        //                                                                      info_logger_,
        //                                                                      in_credit_port.get(),
        //                                                                      out_inst_port.get()));
        //     }
        // }
        

        auto issue_queue_pipe_topology = olympia::coreutils::getPipeTopology(node->getParent(), "issue_queue_topology");

        /*
            pipe_map_ = {"int": ["issue_queue"]}
        */
        // auto setup_pipe_map = [this, issue_queue_pipe_topology, node](std::string exe_unit){
        //     auto alu_pipe_topology = olympia::coreutils::getPipeTopology(node->getParent(), "pipe_topology_" + exe_unit + "_pipes");
        //     for(size_t i = 0; i < alu_pipe_topology.size(); ++i){
        //         std::string unit_name = exe_unit + std::to_string(i);
        //         for(size_t j = 0; j < issue_queue_pipe_topology.size(); ++j){
        //             for(size_t k = 0; k < issue_queue_pipe_topology[j].size(); ++k){
        //                 if(unit_name == issue_queue_pipe_topology[j][k]){
        //                     for(size_t l = 0; l < alu_pipe_topology[i].size(); ++l){
        //                         const auto tgt_pipe = InstArchInfo::execution_pipe_map.find(alu_pipe_topology[i][l]);
        //                         pipe_map_[tgt_pipe->second].insert("issue_queue" + std::to_string(j));
        //                     }
        //                     break;
        //                 }
        //             }
        //         }
        //     }
        // };
        // setup_pipe_map("alu");
        // setup_pipe_map("fpu");
        // setup_pipe_map("br");
        for (size_t i = 0; i < issue_queue_pipe_topology.size(); i++)
        {
            auto exe_units = issue_queue_pipe_topology[i];
            auto exe_unit_first = exe_units[0];
            std::string opts[3] = {"alu", "fpu", "br"};

            for(auto opt: opts){
                size_t found = exe_unit_first.find(opt);
                if (found != std::string::npos)
                {
                    std::string iq_name = "iq" + std::to_string(i);
                    const auto target_itr = InstArchInfo::dispatch_target_map.find(opt);
                    auto & in_credit_port = in_credit_ports_.emplace_back
                        (new sparta::DataInPort<uint32_t>(&unit_port_set_, "in_"+iq_name+"_credits"));
                    in_credit_port->enableCollection(node);

                    auto & out_inst_port = out_inst_ports_.emplace_back
                        (new sparta::DataOutPort<InstQueue::value_type>(&unit_port_set_, "out_"+iq_name+"_write"));
                    dispatchers_[target_itr->second].emplace_back(new Dispatcher(iq_name,
                                                                             this,
                                                                             info_logger_,
                                                                             in_credit_port.get(),
                                                                             out_inst_port.get()));
                }
            }
        }

        /*
            pipe_map_ = {"int": ["issue_queue"]}
        */
        auto setup_pipe_map = [this, issue_queue_pipe_topology, node](std::string exe_unit){
            auto alu_pipe_topology = olympia::coreutils::getPipeTopology(node->getParent(), "pipe_topology_" + exe_unit + "_pipes");
            for(size_t i = 0; i < alu_pipe_topology.size(); ++i){
                std::string unit_name = exe_unit + std::to_string(i);
                for(size_t j = 0; j < issue_queue_pipe_topology.size(); ++j){
                    for(size_t k = 0; k < issue_queue_pipe_topology[j].size(); ++k){
                        if(unit_name == issue_queue_pipe_topology[j][k]){
                            for(size_t l = 0; l < alu_pipe_topology[i].size(); ++l){
                                const auto tgt_pipe = InstArchInfo::execution_pipe_map.find(alu_pipe_topology[i][l]);
                                std::string tgt_unit_name = issue_queue_pipe_topology[j][k];
                                const auto target_itr = InstArchInfo::dispatch_target_map.find(tgt_unit_name.substr(0, tgt_unit_name.size() - 1));
                                auto & dispatchers = dispatchers_[target_itr->second];
                                for(auto & dispatcher: dispatchers){
                                    if(dispatcher->getName() == "iq" + std::to_string(j)){
                                        pipe_map_[tgt_pipe->second].insert(dispatcher);
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
        };
        setup_pipe_map("alu");
        setup_pipe_map("fpu");
        setup_pipe_map("br");

        // Special case for the LSU
        dispatchers_[InstArchInfo::TargetUnit::LSU].emplace_back(new Dispatcher("lsu",
                                                                                this,
                                                                                info_logger_,
                                                                                &in_lsu_credits_,
                                                                                &out_lsu_write_));
        in_lsu_credits_.enableCollection(node);

        in_reorder_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, robCredits_, uint32_t));
        in_reorder_credits_.enableCollection(node);

        in_reorder_flush_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, handleFlush_,
                                                                    FlushManager::FlushingCriteria));
        in_reorder_flush_.enableCollection(node);

        blocking_dispatcher_ = InstArchInfo::TargetUnit::ALU;
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Dispatch, sendInitialCredits_));
    }

    void Dispatch::scheduleDispatchSession()
    {
        if(credits_rob_ > 0 && (dispatch_queue_.size() > 0))
        {
            // See if the one of the original blocking dispatcher
            // types is still blocking.
            if(blocking_dispatcher_ != InstArchInfo::TargetUnit::NONE)
            {
                auto & dispatchers = dispatchers_[blocking_dispatcher_];
                for(auto & disp : dispatchers)
                {
                    if(disp->canAccept()) {
                        blocking_dispatcher_ = InstArchInfo::TargetUnit::NONE;
                        ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
                        break;
                    }
                    else  {
                        ILOG("dispatcher '" << disp->getName() << "' still cannot accept an inst");
                    }
                }
            }
            else {
                ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
            }
        }
        else {
            ILOG("no rob credits or no instructions to process");
        }
    }

    void Dispatch::sendInitialCredits_()
    {
        out_dispatch_queue_credits_.send(dispatch_queue_.capacity());
    }

    void Dispatch::robCredits_(const uint32_t&) {
        uint32_t nc = in_reorder_credits_.pullData();
        credits_rob_ += nc;
        scheduleDispatchSession();
        ILOG("ROB got " << nc << " credits, total: " << credits_rob_);
    }

    void Dispatch::dispatchQueueAppended_(const InstGroupPtr &inst_grp) {
        ILOG("queue appended: " << inst_grp);
        for(auto & i : *in_dispatch_queue_write_.pullData()) {
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

        if(num_dispatch == 0) {
            stall_counters_[current_stall_].startCounting();
            return;
        }

        current_stall_ = NOT_STALLED;

        InstGroupPtr insts_dispatched =
            sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);;
        bool keep_dispatching = true;
        for(uint32_t i = 0; (i < num_dispatch) && keep_dispatching; ++i)
        {
            bool dispatched = false;
            InstPtr & ex_inst_ptr = dispatch_queue_.access(0);
            Inst & ex_inst = *ex_inst_ptr;
            const auto target_unit = ex_inst.getUnit();

            sparta_assert(target_unit != InstArchInfo::TargetUnit::UNKNOWN,
                          "Have an instruction that doesn't know where to go: " << ex_inst);

            if(SPARTA_EXPECT_FALSE(target_unit == InstArchInfo::TargetUnit::ROB))
            {
                ex_inst.setStatus(Inst::Status::COMPLETED);
                // Indicate that this instruction was dispatched
                // -- it goes right to the ROB
                dispatched = true;
            }
            else {
                // Get the dispatchers used to dispatch the target.
                // Find a ready-to-go dispatcher
                auto & dispatchers = dispatchers_[target_unit];
                // so we have a map here that checks for which valid dispatchers for that instruction target pipe
                // map needs to be:
                // "int": [alu0, alu1, alu2, alu3]
                // "mul": [alu0, alu1]
                // disp->getName()
                // ex_inst_ptr->getTarget()
                if(ex_inst_ptr->getUnit() != InstArchInfo::TargetUnit::LSU){
                    auto pipe = ex_inst_ptr->getPipe();
                    auto dispatchers_iq = pipe_map_.find(pipe)->second;
                    uint32_t min_credits = 0;
                    uint32_t idx = 0;
                    uint32_t my_idx = 0;
                    for(auto & dispatcher_iq: dispatchers_iq){
                        if(dispatcher_iq->canAccept()){
                            uint32_t dispatch_creds = dispatcher_iq->getCredits();
                            if(dispatch_creds > min_credits){
                                idx = my_idx;
                                min_credits = dispatch_creds;
                            }
                        }
                        ++my_idx;
                    }
                    if(min_credits != 0){
                        uint32_t curr_idx = 0;
                        for(auto & dispatcher_iq: dispatchers_iq){
                            if(curr_idx == idx){
                                dispatcher_iq->acceptInst(ex_inst_ptr);
                                ++unit_distribution_[target_unit];
                                ++(unit_distribution_context_.context(target_unit));
                                ++(weighted_unit_distribution_context_.context(target_unit));

                                ex_inst_ptr->setStatus(Inst::Status::DISPATCHED);
                                ILOG("Sending instruction: "
                                    << ex_inst_ptr << " to " << dispatcher_iq->getName());
                                dispatched = true;
                            }
                            ++curr_idx;
                        }
                    }
                }
                else{
                    for(auto & disp : dispatchers)
                    {
                        if(disp->canAccept())
                        {
                            disp->acceptInst(ex_inst_ptr);
                            ++unit_distribution_[target_unit];
                            ++(unit_distribution_context_.context(target_unit));
                            ++(weighted_unit_distribution_context_.context(target_unit));

                            ex_inst_ptr->setStatus(Inst::Status::DISPATCHED);
                            ILOG("Sending instruction: "
                                << ex_inst_ptr << " to " << disp->getName());
                            dispatched = true;

                            break;
                        }
                        else {
                            ILOG(disp->getName() << " cannot accept inst: " << ex_inst_ptr);
                            blocking_dispatcher_ = target_unit;
                        }
                    }
                }
                if(false == dispatched) {
                    current_stall_ = static_cast<StallReason>(target_unit);
                    keep_dispatching = false;
                }
            }

            if(dispatched) {
                insts_dispatched->emplace_back(ex_inst_ptr);
                dispatch_queue_.pop();
                --credits_rob_;
            } else {
                ILOG("Could not dispatch: "
                     << ex_inst_ptr << " stall: " << current_stall_);
                break;
            }
        }
        for (auto & dispatchers : dispatchers_) {
            for(auto & disp : dispatchers) {
                disp->reset();
            }
        }

        if(!insts_dispatched->empty()) {
            out_dispatch_queue_credits_.send(static_cast<uint32_t>(insts_dispatched->size()));
            out_reorder_write_.send(insts_dispatched);
        }

        if ((credits_rob_ > 0) && (dispatch_queue_.size() > 0) && (current_stall_ == NOT_STALLED)) {
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
}
