// <Decode.cpp> -*- C++ -*-

#include "Decode.hpp"
#include "fsl_api/FusionTypes.h"

#include "sparta/events/StartupEvent.hpp"
#include "sparta/utils/LogUtils.hpp"

#include <algorithm>
#include <iostream>

using namespace std;

namespace olympia
{
    constexpr char Decode::name[];

    Decode::Decode(sparta::TreeNode* node, const DecodeParameterSet* p) :
        sparta::Unit(node),

        fetch_queue_("FetchQueue", p->fetch_queue_size, node->getClock(), &unit_stat_set_),

        fusion_num_fuse_instructions_(&unit_stat_set_, "fusion_num_fuse_instructions",
                                      "The number of custom instructions created by fusion",
                                      sparta::Counter::COUNT_NORMAL),

        fusion_num_ghost_instructions_(&unit_stat_set_, "fusion_num_ghost_instructions",
                                       "The number of instructions eliminated by fusion",
                                       sparta::Counter::COUNT_NORMAL),

        fusion_num_groups_defined_(&unit_stat_set_, "fusion_num_groups_defined",
                                   "Number of fusion groups compiled or read at run time",
                                   sparta::Counter::COUNT_LATEST),

        fusion_num_groups_utilized_(&unit_stat_set_, "fusion_num_groups_utilized",
                                   "Incremented on first use of a fusion group",
                                   sparta::Counter::COUNT_LATEST),

        fusion_pred_cycles_saved_(&unit_stat_set_, "fusion_pred_cycles_saved",
                                  "Optimistic prediction of the cycles saved by fusion",
                                  sparta::Counter::COUNT_NORMAL),

        num_to_decode_(p->num_to_decode),
        fusion_enable_(p->fusion_enable),
        fusion_debug_(p->fusion_debug),
        fusion_enable_register_(p->fusion_enable_register),
        fusion_max_latency_(p->fusion_max_latency),
        fusion_match_max_tries_(p->fusion_match_max_tries),
        fusion_max_group_size_(p->fusion_max_group_size),
        fusion_summary_report_(p->fusion_summary_report),
        fusion_group_definitions_(p->fusion_group_definitions)
    {
        initializeFusion_();

        fetch_queue_.enableCollection(node);

        fetch_queue_write_in_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Decode, fetchBufferAppended_, InstGroupPtr));
        uop_queue_credits_in_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Decode, receiveUopQueueCredits_, uint32_t));
        in_reorder_flush_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Decode, handleFlush_, FlushManager::FlushingCriteria));

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Decode, sendInitialCredits_));
    }

    // Send fetch the initial credit count
    void Decode::sendInitialCredits_() { fetch_queue_credits_outp_.send(fetch_queue_.capacity()); }

    // -------------------------------------------------------------------
    // -------------------------------------------------------------------
    void Decode::initializeFusion_()
    {
        if (fusion_enable_)
        {
            fuser_  = std::make_unique<FusionType>(fusion_group_definitions_);
            hcache_ = fusion::HCache(FusionGroupType::jenkins_1aat);
            fusion_num_groups_defined_ = fuser_->getFusionGroupContainer().size();
        }
        else
        {
            fuser_ = nullptr;
        }
    }

    // Receive Uop credits from Dispatch
    void Decode::receiveUopQueueCredits_(const uint32_t & credits)
    {
        uop_queue_credits_ += credits;
        if (fetch_queue_.size() > 0)
        {
            ev_decode_insts_event_.schedule(sparta::Clock::Cycle(0));
        }

        ILOG("Received credits: " << uop_queue_credits_in_);
    }

    // Called when the fetch buffer was appended by Fetch.  If decode
    // has the credits, then schedule a decode session.  Otherwise, go
    // to sleep
    void Decode::fetchBufferAppended_(const InstGroupPtr & insts)
    {
        // Cache the instructions in the instruction queue if we can't decode this cycle
        for (auto & i : *insts)
        {
            fetch_queue_.push(i);
            ILOG("Received: " << i);
        }
        if (uop_queue_credits_ > 0)
        {
            ev_decode_insts_event_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Handle incoming flush
    void Decode::handleFlush_(const FlushManager::FlushingCriteria & criteria)
    {
        ILOG("Got a flush call for " << criteria);
        fetch_queue_credits_outp_.send(fetch_queue_.size());
        fetch_queue_.clear();
    }

    // Decode instructions
    void Decode::decodeInsts_()
    {
        uint32_t num_decode = std::min(uop_queue_credits_, fetch_queue_.size());
        num_decode = std::min(num_decode, num_to_decode_);

        // buffer to maximize the chances of a group match limited
        // by max allowed latency, bounded by max group size
        if (fusion_enable_)
        {
            if (num_decode < fusion_max_group_size_ && latency_count_ < fusion_max_latency_)
            {
                ++latency_count_;
                return;
            }
        }

        latency_count_ = 0;

        if (num_decode > 0)
        {
            InstGroupPtr insts =
                sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);

            InstUidListType uids;
            // Send instructions on their way to rename
            for (uint32_t i = 0; i < num_decode; ++i)
            {
                const auto & inst = fetch_queue_.read(0);
                insts->emplace_back(inst);
                inst->setStatus(Inst::Status::DECODED);

                if (fusion_enable_)
                {
                    uids.push_back(inst->getMavisUid());
                }

                ILOG("Decoded: " << inst);

                fetch_queue_.pop();
            }

            if (fusion_enable_)
            {
                MatchInfoListType matches;
                uint32_t max_itrs = 0;
                FusionGroupContainerType & container = fuser_->getFusionGroupContainer();
                do
                {
                    matchFusionGroups_(matches, insts, uids, container);
                    processMatches_(matches, insts, uids);
                    // Future feature whereIsEgon(insts,numGhosts);
                    ++max_itrs;
                } while (matches.size() > 0 && max_itrs < fusion_match_max_tries_);

                if (max_itrs >= fusion_match_max_tries_)
                {
                    throw sparta::SpartaException("Fusion group match watch dog exceeded.");
                }
            }

            // Debug statement
            if (fusion_debug_ && fusion_enable_)
                infoInsts_(cout, insts);
            // Send decoded instructions to rename
            uop_queue_outp_.send(insts);

            // TODO: whereisegon() would remove the ghosts,
            // Commented out for now, in practice insts
            // would be smaller due to the fused ops
            // uint32_t unfusedInstsSize = insts->size();

            // Decrement internal Uop Queue credits
            sparta_assert(uop_queue_credits_ >= insts->size(),
                 "Attempt to decrement d0q credits below what is available");

            uop_queue_credits_ -= insts->size();

            // Send credits back to Fetch to get more instructions
            fetch_queue_credits_outp_.send(insts->size());
        }

        // If we still have credits to send instructions as well as
        // instructions in the queue, schedule another decode session
        if (uop_queue_credits_ > 0 && fetch_queue_.size() > 0)
        {
            ev_decode_insts_event_.schedule(1);
        }
    }
} // namespace olympia
