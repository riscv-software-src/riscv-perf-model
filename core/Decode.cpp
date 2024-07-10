// <Decode.cpp> -*- C++ -*-

#include "Decode.hpp"
#include "VectorUopGenerator.hpp"
#include "fusion/FusionTypes.hpp"

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
        uop_queue_("UOpQueue", p->uop_queue_size, node->getClock(), &unit_stat_set_),
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
        in_vset_inst_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Decode, process_vset_, InstPtr));

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Decode, sendInitialCredits_));

        VCSRs_.setVCSRs(p->init_vl, p->init_sew, p->init_lmul, p->init_vta);
    }

    // Send fetch the initial credit count
    void Decode::sendInitialCredits_()
    {
        fetch_queue_credits_outp_.send(fetch_queue_.capacity());

        // setting MavisIDs for vsetvl and vsetivli
        mavis_facade_ = getMavis(getContainer());
        mavis_vsetvl_uid_ = mavis_facade_->lookupInstructionUniqueID("vsetvl");
        mavis_vsetivli_uid_ = mavis_facade_->lookupInstructionUniqueID("vsetivli");
        mavis_vsetvli_uid_ = mavis_facade_->lookupInstructionUniqueID("vsetvli");

        // Get pointer to the vector uop generator
        sparta::TreeNode * root_node = getContainer()->getRoot();
        vec_uop_gen_ = \
            root_node->getChild("cpu.core0.decode.vec_uop_gen")->getResourceAs<olympia::VectorUopGenerator*>();
        vec_uop_gen_->setMavis(mavis_facade_);
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

    void Decode::updateVcsrs_(const InstPtr & inst)
    {
        VCSRs_.setVCSRs(inst->getVL(), inst->getSEW(), inst->getLMUL(), inst->getVTA());

	const uint64_t uid = inst->getOpCodeInfo()->getInstructionUniqueID();
        if ((uid == mavis_vsetvli_uid_) && inst->hasZeroRegSource())
        {
            // If rs1 is x0 and rd is x0 then the vl is unchanged (assuming it is legal)
            // If rs1 is x0 and rd is not x0 then vl is set to vlmax
            VCSRs_.vl = inst->hasZeroRegDest() ? std::min(VCSRs_.vl, VCSRs_.vlmax)
	                                       : VCSRs_.vlmax;
        }

        ILOG("Processing vset{i}vl{i} instruction: " << inst);
        ILOG("  LMUL: " << VCSRs_.lmul);
        ILOG("   SEW: " << VCSRs_.sew);
        ILOG("   VTA: " << VCSRs_.vta);
        ILOG(" VLMAX: " << VCSRs_.vlmax);
        ILOG("    VL: " << VCSRs_.vl);

        // Check validity of vector config
        sparta_assert(VCSRs_.lmul <= 8,
            "LMUL (" << VCSRs_.lmul << ") cannot be greater than " << 8);
        sparta_assert(VCSRs_.vl <= VCSRs_.vlmax,
            "VL (" << VCSRs_.vl << ") cannot be greater than VLMAX ("<< VCSRs_.vlmax << ")");
    }

    // process vset settings being forward from execution pipe
    // for set instructions that depend on register
    void Decode::process_vset_(const InstPtr & inst)
    {
        updateVcsrs_(inst);

        // if rs1 != 0, VL = x[rs1], so we assume there's an STF field for VL
        if (waiting_on_vset_)
        {
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
    }

    // Decode instructions
    void Decode::decodeInsts_()
    {
        uint32_t num_to_decode = std::min(uop_queue_credits_, fetch_queue_.size() + uop_queue_.size());
        num_to_decode = std::min(num_to_decode, num_to_decode_);

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
        InstGroupPtr insts =
            sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);
        // if we have a waiting on vset followed by more instructions, we decode
        // vset and stall anything else
        while ((insts->size() < num_to_decode) && !waiting_on_vset_)
        {
            if (uop_queue_.size() > 0)
            {
                const auto & inst = uop_queue_.read(0);
                insts->emplace_back(inst);
                inst->setStatus(Inst::Status::DECODED);
                ILOG("From UOp Queue Decoded: " << inst);
                uop_queue_.pop();
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
                if ((uid == mavis_vsetivli_uid_) ||
                   ((uid == mavis_vsetvli_uid_) && inst->hasZeroRegSource()))
                {
                    updateVcsrs_(inst);
                }
                else if (uid == mavis_vsetvli_uid_ || uid == mavis_vsetvl_uid_)
                {
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
                        inst->setVCSRs(&VCSRs_);
                    }
                }

                ILOG("Decoded: " << inst);
                insts->emplace_back(inst);
                inst->setStatus(Inst::Status::DECODED);

                // Handle vector uop generation
                if (inst->isVector() && !inst->isVset())
                {
                    // If LMUL > 1, fracture instruction into UOps
                    ILOG("Vector uop gen: " << inst);
                    vec_uop_gen_->setInst(inst);

                    // Original instruction will act as the first UOp
                    inst->setUOpID(0); // set UOpID()

                    while(vec_uop_gen_->keepGoing())
                    {
                        const InstPtr uop = vec_uop_gen_->genUop();
                        if (insts->size() < num_to_decode_)
                        {
                            insts->emplace_back(uop);
                            uop->setStatus(Inst::Status::DECODED);
                        }
                        else
                        {
                            ILOG("Not enough decode credits to process UOp, "
                                 "appending to the uop queue: " << uop);
                            uop_queue_.push(uop);
                        }
                    }
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
                // Uop queue and fetch queue are both empty, nothing left to decode
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
        sparta_assert(insts->size() <= num_to_decode_,
            "Instruction group grew too large! " << insts);
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

        // If we still have credits to send instructions as well as
        // instructions in the queue, schedule another decode session
        if (uop_queue_credits_ > 0 && (fetch_queue_.size() + uop_queue_.size()) > 0)
        {
            ev_decode_insts_event_.schedule(1);
        }
    }
} // namespace olympia
