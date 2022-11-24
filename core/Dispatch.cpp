// <Dispatch.cpp> -*- C++ -*-

#include <algorithm>
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

        in_fpu_credits_.enableCollection(node);
        in_alu_credits_.enableCollection(node);
        in_br_credits_.enableCollection(node);
        in_lsu_credits_.enableCollection(node);

        dispatchers_[InstArchInfo::TargetUnit::ALU].emplace_back(new Dispatcher("alu",
                                                                                this,
                                                                                info_logger_,
                                                                                in_alu_credits_,
                                                                                out_alu_write_));
        dispatchers_[InstArchInfo::TargetUnit::FPU].emplace_back(new Dispatcher("fpu",
                                                                                this,
                                                                                info_logger_,
                                                                                in_fpu_credits_,
                                                                                out_fpu_write_));
        dispatchers_[InstArchInfo::TargetUnit::BR].emplace_back(new Dispatcher("br",
                                                                               this,
                                                                               info_logger_,
                                                                               in_br_credits_,
                                                                               out_br_write_));

        dispatchers_[InstArchInfo::TargetUnit::LSU].emplace_back(new Dispatcher("lsu",
                                                                                this,
                                                                                info_logger_,
                                                                                in_lsu_credits_,
                                                                                out_lsu_write_));

        in_reorder_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, robCredits_, uint32_t));
        in_reorder_credits_.enableCollection(node);

        in_reorder_flush_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, handleFlush_, FlushManager::FlushingCriteria));
        in_reorder_flush_.enableCollection(node);

        blocking_dispatcher_ = dispatchers_[InstArchInfo::TargetUnit::ALU][0].get();
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Dispatch, sendInitialCredits_));
    }

    void Dispatch::scheduleDispatchSession()
    {
        if(credits_rob_ > 0 && (dispatch_queue_.size() > 0))
        {
            if(blocking_dispatcher_)
            {
                if(blocking_dispatcher_->canAccept()) {
                    blocking_dispatcher_ = nullptr;
                    ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
                }
                else if(SPARTA_EXPECT_FALSE(info_logger_)) {
                    info_logger_ << "dispatcher '" << blocking_dispatcher_->getName() << "' is blocking dispatching";
                }
            }
            else {
                ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
            }
        }
        else if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "no rob credits or no instructions to process";
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
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "ROB got " << nc << " credits, total: " << credits_rob_;
        }
    }

    void Dispatch::dispatchQueueAppended_(const InstGroupPtr &inst_grp) {
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "queue appended: " << inst_grp;
        }
        for(auto & i : *in_dispatch_queue_write_.pullData()) {
            dispatch_queue_.push(i);
        }
        scheduleDispatchSession();
    }

    void Dispatch::dispatchInstructions_()
    {
        uint32_t num_dispatch = std::min(dispatch_queue_.size(), num_to_dispatch_);
        num_dispatch = std::min(credits_rob_, num_dispatch);

        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Num to dispatch: " << num_dispatch;
        }

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
                for(auto & disp : dispatchers)
                {
                    if(disp->canAccept())
                    {
                        disp->acceptInst(ex_inst_ptr);
                        ++unit_distribution_[target_unit];
                        ++(unit_distribution_context_.context(target_unit));
                        ++(weighted_unit_distribution_context_.context(target_unit));

                        if(SPARTA_EXPECT_FALSE(info_logger_)) {
                            info_logger_ << "Sending instruction: "
                                         << ex_inst_ptr << " to " << disp->getName();
                        }
                        dispatched = true;
                        break;
                    }
                    else {
                        if(SPARTA_EXPECT_FALSE(info_logger_)) {
                            info_logger_ << disp->getName() << " cannot accept inst: " << ex_inst_ptr;
                        }
                        blocking_dispatcher_ = disp.get();
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
                if(SPARTA_EXPECT_FALSE(info_logger_)) {
                    info_logger_ << "Could not dispatch: "
                                 << ex_inst_ptr << " stall: " << current_stall_;
                }
                break;
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
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Got a flush call for " << criteria;
        }
        out_dispatch_queue_credits_.send(dispatch_queue_.size());
        dispatch_queue_.clear();
        out_reorder_write_.cancel();
    }
}
