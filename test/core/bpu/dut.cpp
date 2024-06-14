#include "sparta/events/StartupEvent.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "dut.h"
#include <algorithm>
#include <iostream>
using namespace std;

namespace olympia
{
    constexpr char Dut::name[];

    Dut::Dut(sparta::TreeNode* node, const DutParameterSet* p) :
        sparta::Unit(node),
        input_queue_("FetchQueue", p->input_queue_size, node->getClock(),
                     &unit_stat_set_),
        num_to_process_(p->num_to_process)
    {
        input_queue_.enableCollection(node);

        i_instgrp_write_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Dut, inputQueueAppended_,
                                            InstGroupPtr));
        i_credits_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Dut, receiveInpQueueCredits_,
                                            uint32_t));
        i_dut_flush_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(
                Dut, handleFlush_, FlushManager::FlushingCriteria));

        i_bpu_response_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Dut, receiveBpuResponse,
                                            BpuResponseInfo));
        sparta::StartupEvent(
            node, CREATE_SPARTA_HANDLER(Dut, sendInitialCredits_));
    }

    // Send source the initial credit count
    void Dut::sendInitialCredits_()
    {
        o_restore_credits_.send(input_queue_.capacity());
    }

    // Receive Uop credits from Dispatch
    void Dut::receiveInpQueueCredits_(const uint32_t & credits)
    {
        inp_queue_credits_ += credits;
        if (input_queue_.size() > 0)
        {
            ev_process_insts_event_.schedule(sparta::Clock::Cycle(0));
        }

        ILOG("Received credits: " << i_credits_);
    }

    // Called when the input buffer was appended by source.  If dut
    // has the credits, then schedule a processing session.  Otherwise,
    // go to sleep
    void Dut::inputQueueAppended_(const InstGroupPtr & insts)
    {
        // Cache the instructions in the input queue if we can't
        // process them this cycle
        for (auto & i : *insts)
        {
            input_queue_.push(i);
            ILOG("Received: " << i);
        }
        if (inp_queue_credits_ > 0)
        {
            ev_process_insts_event_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Handle incoming flush
    void Dut::handleFlush_(const FlushManager::FlushingCriteria & criteria)
    {
        ILOG("Got a flush call for " << criteria);
        o_restore_credits_.send(input_queue_.size());
        input_queue_.clear();
    }

    void Dut::processInsts_()
    {
        uint32_t num_process =
            std::min(inp_queue_credits_, input_queue_.size());
        num_process = std::min(num_process, num_to_process_);

        if (num_process > 0)
        {
            InstGroupPtr insts =
                sparta::allocate_sparta_shared_pointer<InstGroup>(
                    instgroup_allocator);

            // Send instructions on their way to sink unit
            for (uint32_t i = 0; i < num_process; ++i)
            {
                const auto & inst = input_queue_.read(0);
                insts->emplace_back(inst);
                //This is a placeholder, there isn't a more accurate state
                //defined in the existing Inst.h.
                inst->setStatus(Inst::Status::RENAMED);
                ILOG("Dut: " << inst);
                input_queue_.pop();
            }

            // Send processed instructions to sink
            o_instgrp_write_.send(insts);

            // Decrement internal Uop Queue credits
            inp_queue_credits_ -= num_process;

            // Send credits back to Fetch to get more instructions
            o_restore_credits_.send(num_process);
        }

        // If we still have credits to send instructions as well as
        // instructions in the queue, schedule another processing session
        if (inp_queue_credits_ > 0 && input_queue_.size() > 0)
        {
            ev_process_insts_event_.schedule(1);
        }
    }
} // namespace olympia
