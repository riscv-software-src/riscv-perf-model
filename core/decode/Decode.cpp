// <Decode.cpp> -*- C++ -*-

#include "decode/Decode.hpp"
#include "vector/VectorUopGenerator.hpp"
#include "fsl_api/FusionTypes.h"

#include "sparta/events/StartupEvent.hpp"
#include "sparta/statistics/Counter.hpp"
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
        fusion_group_definitions_(p->fusion_group_definitions),
        vector_enabled_(true),
        vector_config_(new VectorConfig(p->init_vl, p->init_sew, p->init_lmul, p->init_vta)),
        vset_blocking_count_(&unit_stat_set_, "vset_blocking_count",
                             "Number of times that the Decode unit blocks execution",
                             sparta::Counter::COUNT_NORMAL),
        vset_blocking_stall_latency_(&unit_stat_set_, "vset_blocking_stall_latency",
                                     "Accumulated between roundtrip vset decode and processing",
                                     sparta::Counter::COUNT_NORMAL)
    {
        initializeFusion_();

        fetch_queue_.enableCollection(node);

        fetch_queue_write_in_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Decode, fetchBufferAppended_, InstGroupPtr));
        uop_queue_credits_in_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Decode, receiveUopQueueCredits_, uint32_t));
        in_reorder_flush_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Decode, handleFlush_, FlushManager::FlushingCriteria));
        in_vset_inst_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Decode, processVset_, InstPtr));

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Decode, sendInitialCredits_));
    }

    uint32_t Decode::getNumVecUopsRemaining() const
    {
        return vector_enabled_ ? vec_uop_gen_->getNumUopsRemaining() : 0;
    }

    // Send fetch the initial credit count
    void Decode::sendInitialCredits_()
    {
        fetch_queue_credits_outp_.send(fetch_queue_.capacity());

        // Get pointer to the vector uop generator
        sparta::TreeNode* root_node = getContainer()->getRoot();
        vec_uop_gen_ = root_node->getChild("cpu.core0.decode.vec_uop_gen")
                           ->getResourceAs<olympia::VectorUopGenerator*>();
    }

    // -------------------------------------------------------------------
    // -------------------------------------------------------------------
    void Decode::initializeFusion_()
    {
        if (fusion_enable_)
        {
            fuser_ = std::make_unique<FusionType>(fusion_group_definitions_);
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

    void Decode::updateVectorConfig_(const InstPtr & inst)
    {
        vector_config_ = inst->getVectorConfig();

        // If rs1 is x0 and rd is x0 then the vl is unchanged (assuming it is legal)
        const uint64_t uid = inst->getOpCodeInfo()->getInstructionUniqueID();
        if ((uid == MAVIS_UID_VSETVLI) && inst->hasZeroRegSource())
        {
            const uint32_t new_vl = inst->hasZeroRegDest() ? std::min(vector_config_->getVL(),
                                                                      vector_config_->getVLMAX())
                                                           : vector_config_->getVLMAX();
            vector_config_->setVL(new_vl);
        }

        ILOG("Processing vset{i}vl{i} instruction: " << inst << " " << vector_config_);
    }

    // process vset settings being forward from execution pipe
    // for set instructions that depend on register
    void Decode::processVset_(const InstPtr & inst)
    {
        updateVectorConfig_(inst);

        // if rs1 != 0, VL = x[rs1], so we assume there's an STF field for VL
        if (waiting_on_vset_)
        {
            const auto vset_block_end = getClock()->currentCycle();
            vset_blocking_stall_latency_ += (vset_block_end - vset_block_start_);
            // schedule decode, because we've been stalled on vset
            waiting_on_vset_ = false;
            ev_decode_insts_event_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Handle incoming flush
    void Decode::handleFlush_(const FlushManager::FlushingCriteria & criteria)
    {
        ILOG("Got a flush call for " << criteria);
        fetch_queue_credits_outp_.send(fetch_queue_.size());
        fetch_queue_.clear();

        // Reset the vector uop generator
        vec_uop_gen_->handleFlush(criteria);
    }

    // Decode instructions
    void Decode::decodeInsts_()
    {
        const uint32_t num_to_decode = std::min(uop_queue_credits_, num_to_decode_);

        // buffer to maximize the chances of a group match limited
        // by max allowed latency, bounded by max group size
        if (fusion_enable_)
        {
            if (num_to_decode < fusion_max_group_size_ && latency_count_ < fusion_max_latency_)
            {
                ++latency_count_;
                return;
            }
        }

        latency_count_ = 0;

        // For fusion
        InstUidListType uids;

        // Send instructions on their way to rename
        InstGroupPtr insts = sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);
        // if we have a waiting on vset followed by more instructions, we decode
        // vset and stall anything else
        while ((insts->size() < num_to_decode) && !waiting_on_vset_)
        {
            if (vec_uop_gen_->getNumUopsRemaining() > 0)
            {
                const InstPtr uop = vec_uop_gen_->generateUop();
                insts->emplace_back(uop);
                uop->setStatus(Inst::Status::DECODED);
            }
            else if (fetch_queue_.size() > 0)
            {
                sparta_assert(fetch_queue_.size() > 0,
                              "Cannot read from the fetch queue because it is empty!");
                auto & inst = fetch_queue_.read(0);

                // for vector instructions, we block on vset and do not allow any other
                // processing of instructions until the vset is resolved optimizations could be
                // to allow scalar operations to move forward until a subsequent vector
                // instruction is detected or do vset prediction

                // vsetvl always block
                // vsetvli only blocks if rs1 is not x0
                // vsetivli never blocks
                const uint64_t uid = inst->getOpCodeInfo()->getInstructionUniqueID();
                if ((uid == MAVIS_UID_VSETIVLI)
                    || ((uid == MAVIS_UID_VSETVLI) && inst->hasZeroRegSource()))
                {
                    updateVectorConfig_(inst);
                }
                else if (uid == MAVIS_UID_VSETVLI || uid == MAVIS_UID_VSETVL)
                {
                    ++vset_blocking_count_;

                    vset_block_start_ = getClock()->currentCycle();
                    // block for vsetvl or vsetvli when rs1 of vsetvli is NOT 0
                    waiting_on_vset_ = true;
                    // need to indicate we want a signal sent back at execute
                    inst->setBlockingVSET(true);
                    ILOG("Decode stalled, Waiting on vset that has register dependency: " << inst)
                }
                else
                {
                    if (!inst->isVset() && inst->isVector())
                    {
                        // set LMUL, VSET, VL, VTA for any other vector instructions
                        inst->setVectorConfig(vector_config_);
                    }
                }

                ILOG("Decoded: " << inst);

                // Even if LMUL == 1, we need the vector uop generator to create a uop for us
                // because some generators will add additional sources and destinations to the
                // instruction (e.g. widening, multiply-add, slides).
                if (inst->isVector() && !inst->isVset()
                    && (inst->getUopGenType() != InstArchInfo::UopGenType::NONE))
                {
                    ILOG("Vector uop gen: " << inst);
                    vec_uop_gen_->setInst(inst);

                    const InstPtr uop = vec_uop_gen_->generateUop();
                    insts->emplace_back(uop);
                    uop->setStatus(Inst::Status::DECODED);
                }
                else
                {
                    insts->emplace_back(inst);
                    inst->setStatus(Inst::Status::DECODED);
                }

                if (fusion_enable_)
                {
                    uids.push_back(inst->getMavisUid());
                }

                // Remove from Fetch Queue
                fetch_queue_.pop();
            }
            else
            {
                // Nothing left to decode
                break;
            }
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

            // Debug statement
            if (fusion_debug_)
            {
                infoInsts_(cout, insts);
            }
        }

        // Send decoded instructions to rename
        uop_queue_outp_.send(insts);

        // TODO: whereisegon() would remove the ghosts,
        // Commented out for now, in practice insts
        // would be smaller due to the fused ops
        // uint32_t unfusedInstsSize = insts->size();

        // Decrement internal Uop Queue credits
        uop_queue_credits_ -= insts->size();

        // Send credits back to Fetch to get more instructions
        fetch_queue_credits_outp_.send(insts->size());

        // If we still have credits to send instructions as well as
        // instructions in the queue, schedule another decode session
        if (uop_queue_credits_ > 0 && (fetch_queue_.size() + getNumVecUopsRemaining()) > 0)
        {
            ev_decode_insts_event_.schedule(1);
        }
    }
} // namespace olympia
