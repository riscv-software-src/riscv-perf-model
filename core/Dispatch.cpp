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

        in_fpu_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, fpuCredits_, uint32_t));
        in_fpu_credits_.enableCollection(node);

        in_alu_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, aluCredits_, uint32_t));
        in_alu_credits_.enableCollection(node);

        in_br_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, brCredits_, uint32_t));
        in_br_credits_.enableCollection(node);

        in_lsu_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, lsuCredits_, uint32_t));
        in_lsu_credits_.enableCollection(node);

        in_reorder_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, robCredits_, uint32_t));
        in_reorder_credits_.enableCollection(node);

        in_reorder_flush_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, handleFlush_, FlushManager::FlushingCriteria));
        in_reorder_flush_.enableCollection(node);

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Dispatch, sendInitialCredits_));
    }

    void Dispatch::sendInitialCredits_()
    {
        out_dispatch_queue_credits_.send(dispatch_queue_.capacity());
    }

    void Dispatch::fpuCredits_ (const uint32_t& credits) {
        credits_fpu_ += credits;
        if (credits_rob_ >0 && dispatch_queue_.size() > 0) {
            ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
        }
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "FPU got " << credits << " credits, total: " << credits_fpu_;
        }
    }

    void Dispatch::aluCredits_ (const uint32_t& credits) {
        credits_alu_ += credits;
        if (credits_rob_ >0 && dispatch_queue_.size() > 0) {
            ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
        }
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "ALU got " << credits << " credits, total: " << credits_alu_;
        }
    }

    void Dispatch::brCredits_ (const uint32_t& credits) {
        credits_br_ += credits;
        if (credits_rob_ >0 && dispatch_queue_.size() > 0) {
            ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
        }
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "BR got " << credits << " credits, total: " << credits_br_;
        }
    }

    void Dispatch::lsuCredits_(const uint32_t& credits) {
        credits_lsu_ += credits;
        if (credits_rob_ >0 && dispatch_queue_.size() > 0) {
            ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
        }
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "LSU got " << credits << " credits, total: " << credits_lsu_;
        }
    }

    void Dispatch::robCredits_(const uint32_t&) {
        uint32_t nc = in_reorder_credits_.pullData();
        credits_rob_ += nc;
        if (((credits_fpu_ > 0)|| (credits_alu_ > 0) || (credits_br_ > 0))
            && dispatch_queue_.size() > 0) {
            ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
        }
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

        if (((credits_fpu_ > 0)|| (credits_alu_ > 0) || (credits_br_ > 0) || credits_lsu_ > 0)
            && credits_rob_ > 0) {
            ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
        }
        else if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "no credits in any unit -- not dispatching";
        }
    }

    void Dispatch::handleFlush_(const FlushManager::FlushingCriteria & criteria)
    {
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Got a flush call for " << criteria;
        }
        out_dispatch_queue_credits_.send(dispatch_queue_.size());
        dispatch_queue_.clear();
        credits_fpu_  += out_fpu_write_.cancel();
        credits_alu_ += out_alu_write_.cancel();
        credits_br_   += out_br_write_.cancel();
        credits_lsu_  += out_lsu_write_.cancel();
        out_reorder_write_.cancel();
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

            switch(ex_inst.getUnit())
            {
            case InstArchInfo::TargetUnit::FPU:
                {
                    if(credits_fpu_ > 0) {
                        --credits_fpu_;
                        dispatched = true;
                        out_fpu_write_.send(ex_inst_ptr);
                        ++unit_distribution_[static_cast<uint32_t>(InstArchInfo::TargetUnit::FPU)];
                        ++(unit_distribution_context_.context(static_cast<uint32_t>(InstArchInfo::TargetUnit::FPU)));
                        ++(weighted_unit_distribution_context_.context(static_cast<uint32_t>(InstArchInfo::TargetUnit::FPU)));

                        if(SPARTA_EXPECT_FALSE(info_logger_)) {
                            info_logger_ << "Sending instruction: "
                                         << ex_inst_ptr << " to FPU ";
                        }
                    }
                    else {
                        current_stall_ = FPU_BUSY;
                        keep_dispatching = false;
                    }
                }
                break;
            case InstArchInfo::TargetUnit::ALU:
                {
                    if(credits_alu_ > 0) {
                        --credits_alu_;
                        dispatched = true;
                        //out_alu_write_.send(ex_inst_ptr);  // <- This will cause an assert in the Port!
                        out_alu_write_.send(ex_inst_ptr, 1);
                        ++unit_distribution_[static_cast<uint32_t>(InstArchInfo::TargetUnit::ALU)];
                        ++(unit_distribution_context_.context(static_cast<uint32_t>(InstArchInfo::TargetUnit::ALU)));
                        ++(weighted_unit_distribution_context_.context(static_cast<uint32_t>(InstArchInfo::TargetUnit::ALU)));
                        ++(alu_context_.context(0));

                        if(SPARTA_EXPECT_FALSE(info_logger_)) {
                            info_logger_ << "Sending instruction: "
                                         << ex_inst_ptr << " to ALU ";
                        }
                    }
                    else {
                        current_stall_ = ALU_BUSY;
                        keep_dispatching = false;
                    }
                }
                break;
             case InstArchInfo::TargetUnit::BR:
                {
                    if(credits_br_ > 0)
                    {
                        --credits_br_;
                        dispatched = true;
                        out_br_write_.send(ex_inst_ptr, 1);
                        ++unit_distribution_[static_cast<uint32_t>(InstArchInfo::TargetUnit::BR)];
                        ++(unit_distribution_context_.context(static_cast<uint32_t>(InstArchInfo::TargetUnit::BR)));
                        ++(weighted_unit_distribution_context_.context(static_cast<uint32_t>(InstArchInfo::TargetUnit::BR)));

                        if(SPARTA_EXPECT_FALSE(info_logger_)) {
                            info_logger_ << "Sending instruction: "
                                         << ex_inst_ptr << " to BR ";
                        }
                    }
                    else {
                        current_stall_ = BR_BUSY;
                        keep_dispatching = false;
                    }
                }
                break;
             case InstArchInfo::TargetUnit::LSU:
                {
                    if(credits_lsu_ > 0)
                    {
                        --credits_lsu_;
                        dispatched = true;
                        out_lsu_write_.send(ex_inst_ptr, 1);
                        ++unit_distribution_[static_cast<uint32_t>(InstArchInfo::TargetUnit::LSU)];
                        ++(unit_distribution_context_.context(static_cast<uint32_t>(InstArchInfo::TargetUnit::LSU)));
                        ++(weighted_unit_distribution_context_.context(static_cast<uint32_t>(InstArchInfo::TargetUnit::LSU)));

                        if(SPARTA_EXPECT_FALSE(info_logger_)) {
                            info_logger_ << "sending instruction: "
                                         << ex_inst_ptr << " to LSU ";
                        }
                    }
                    else {
                        current_stall_ = LSU_BUSY;
                        keep_dispatching = false;
                    }
                }
                break;
             case InstArchInfo::TargetUnit::ROB:
                {
                    ex_inst.setStatus(Inst::Status::COMPLETED);
                    // Indicate that this instruction was dispatched
                    // -- it goes right to the ROB
                    dispatched = true;
                }
                break;
            default:
                sparta_assert(false, "Unknown target for instruction: " << ex_inst);
            }

            if(dispatched) {
                insts_dispatched->emplace_back(ex_inst_ptr);
                dispatch_queue_.pop();
                --credits_rob_;
            } else {
                if(SPARTA_EXPECT_FALSE(info_logger_)) {
                    info_logger_ << "Could not dispatch: "
                                   << ex_inst_ptr
                                 << " ALU_B(" << std::boolalpha << (credits_alu_ == 0)
                                 << ") FPU_B(" <<  (credits_fpu_ == 0)
                                 << ") BR_B(" <<  (credits_br_ == 0) << ")";
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
}
