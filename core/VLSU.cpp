#include "sparta/utils/SpartaAssert.hpp"
#include "CoreUtils.hpp"
#include "VLSU.hpp"
#include "sparta/simulation/Unit.hpp"
#include <string>
#include "Decode.hpp"

#include "OlympiaAllocators.hpp"

namespace olympia
{
    const char VLSU::name[] = "VLSU";

    ////////////////////////////////////////////////////////////////////////////////
    // Constructor
    ////////////////////////////////////////////////////////////////////////////////

    VLSU::VLSU(sparta::TreeNode* node, const VLSUParameterSet* p) :
        LSU(node, p),
        mem_req_buffer_(node->getName() + "_mem_req_buffer", p->mem_req_buffer_size, getClock()),
        mem_req_buffer_size_(p->mem_req_buffer_size),
        data_width_(p->data_width)
    {
        // Generated memory requests are appended directly to the ready queue
        uev_gen_mem_ops_ >> uev_issue_inst_;
    }

    void VLSU::onStartingTeardown_()
    {
        // If ROB has not stopped the simulation &
        // the ldst has entries to process we should fail
        if (!rob_stopped_simulation_ &&
            ((mem_req_buffer_.empty() == false) || (inst_queue_.empty() == false)))
        {
            dumpDebugContent_(std::cerr);
            sparta_assert(false, "Issue queue has pending instructions");
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Callbacks
    ////////////////////////////////////////////////////////////////////////////////

    // Generate memory requests for a vector load or store
    void VLSU::genMemoryRequests_()
    {
        // Nothing to do
        // TODO: assert?
        if (mem_req_ready_queue_.empty())
        {
            return;
        }

        // No room in the memory request buffer for new requests
        if (mem_req_buffer_.size() == mem_req_buffer_size_)
        {
            ILOG("Not enough space in the memory request buffer")
            return;
        }

        const InstPtr & inst_ptr = mem_req_ready_queue_.top()->getInstPtr();
        VectorMemConfigPtr vector_mem_config_ptr = inst_ptr->getVectorMemConfig();

        // Get the access width
        const uint32_t width = std::min(data_width_, vector_mem_config_ptr->getEew());
        sparta_assert(width != 0, "VLSU data width cannot be zero!");

        // TODO: Consider VL when generating memory requests
        if (vector_mem_config_ptr->getTotalMemReqs() == 0)
        {
            ILOG("Starting memory request generation for " << inst_ptr);
            vector_mem_config_ptr->setTotalMemReqs(VectorConfig::VLEN / width);
        }

        const uint32_t total_mem_reqs = vector_mem_config_ptr->getTotalMemReqs();
        for (uint32_t mem_req_num = vector_mem_config_ptr->getNumMemReqsGenerated(); mem_req_num < total_mem_reqs; ++mem_req_num)
        {
            if (mem_req_buffer_.size() < mem_req_buffer_size_)
            {
                // TODO: Address Unroller Class, strided and indexed loads/stores are not supported
                // FIXME: Consider uop id
                sparta::memory::addr_t vaddr = inst_ptr->getTargetVAddr() +
                    (mem_req_num * vector_mem_config_ptr->getStride());
                sparta::memory::addr_t paddr = inst_ptr->getPAddr() +
                    (mem_req_num * vector_mem_config_ptr->getStride());

                // Create LS inst info
                LoadStoreInstInfoPtr lsinfo_inst_ptr = createLoadStoreInst_(inst_ptr);
                lsinfo_inst_ptr->getMemoryAccessInfoPtr()->setVAddr(vaddr);
                lsinfo_inst_ptr->getMemoryAccessInfoPtr()->setPAddr(paddr);
                lsinfo_inst_ptr->setState(LoadStoreInstInfo::IssueState::READY);

                // Append to the memory request buffer
                const LoadStoreInstIterator & iter = mem_req_buffer_.push_back(lsinfo_inst_ptr);
                lsinfo_inst_ptr->setMemoryRequestBufferIterator(iter);

                // Increment count of memory requests generated
                vector_mem_config_ptr->incrementNumMemReqsGenerated();
                DLOG("Generating request: "
                     << mem_req_num << " of " << total_mem_reqs << " for " << inst_ptr
                     << " (vaddr: 0x" << std::hex << vaddr << ")");

                // Appending to ready queue
                appendToReadyQueue_(lsinfo_inst_ptr);

                // Done generating memory requests for this vector instruction
                if (mem_req_num + 1 == total_mem_reqs)
                {
                    ILOG("Done with memory request generation for " << inst_ptr);
                    mem_req_ready_queue_.pop();
                }
            }
            else
            {
                ILOG("Not enough space in the memory request buffer")
                break;
            }
        }

        if (mem_req_ready_queue_.size() > 0)
        {
            uev_gen_mem_ops_.schedule(sparta::Clock::Cycle(1));
        }
        if (isReadyToIssueInsts_())
        {
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Callback from Scoreboard to inform Operand Readiness
    void VLSU::handleOperandIssueCheck_(const LoadStoreInstInfoPtr & lsinst_info_ptr)
    {
        const auto inst_ptr = lsinst_info_ptr->getInstPtr();
        if (inst_ptr->getStatus() == Inst::Status::SCHEDULED)
        {
            ILOG("Instruction was previously ready " << inst_ptr);
            return;
        }

        bool all_ready = true; // assume all ready
        // address operand check
        if (!instOperandReady_(inst_ptr))
        {
            all_ready = false;
            const auto & src_bits = inst_ptr->getSrcRegisterBitMask(core_types::RF_INTEGER);
            scoreboard_views_[core_types::RF_INTEGER]->registerReadyCallback(
                src_bits, inst_ptr->getUniqueID(),
                [this, lsinst_info_ptr](const sparta::Scoreboard::RegisterBitMask &)
                { this->handleOperandIssueCheck_(lsinst_info_ptr); });
            ILOG("Instruction NOT ready: " << inst_ptr << " Address Bits needed:"
                                           << sparta::printBitSet(src_bits));
        }
        else
        {
            // we wait for address operand to be ready before checking data operand in the case of
            // stores this way we avoid two live callbacks
            if (inst_ptr->isStoreInst())
            {
                const auto rf = inst_ptr->getRenameData().getDataReg().rf;
                const auto & data_bits = inst_ptr->getDataRegisterBitMask(rf);
                // if x0 is a data operand, we don't need to check scoreboard
                if (!inst_ptr->getRenameData().getDataReg().is_x0)
                {
                    if (!scoreboard_views_[rf]->isSet(data_bits))
                    {
                        all_ready = false;
                        scoreboard_views_[rf]->registerReadyCallback(
                            data_bits, inst_ptr->getUniqueID(),
                            [this, lsinst_info_ptr](const sparta::Scoreboard::RegisterBitMask &)
                            { this->handleOperandIssueCheck_(lsinst_info_ptr); });
                        ILOG("Instruction NOT ready: " << inst_ptr << " Bits needed:"
                                                       << sparta::printBitSet(data_bits));
                    }
                }
            }
            else if (false == allow_speculative_load_exec_)
            { // Its a load
                // Load instruction is ready is when both address and older stores addresses are
                // known
                all_ready = allOlderStoresIssued_(inst_ptr);
            }
        }

        // Load are ready when operands are ready
        // Stores are ready when both operands and data is ready
        // If speculative loads are allowed older store are not checked for Physical address
        if (all_ready)
        {
            // Update issue priority & Schedule an instruction issue event
            updateIssuePriorityAfterNewDispatch_(inst_ptr);

            // Start generating memory requests
            mem_req_ready_queue_.insert(lsinst_info_ptr);
            uev_gen_mem_ops_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Retire load/store instruction
    void VLSU::completeInst_()
    {
        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(complete_stage_))
        {
            return;
        }

        const LoadStoreInstInfoPtr & lsinfo_inst_ptr = ldst_pipeline_[complete_stage_];
        const MemoryAccessInfoPtr & mem_access_info_ptr =
            lsinfo_inst_ptr->getMemoryAccessInfoPtr();

        if (false == mem_access_info_ptr->isDataReady())
        {
            ILOG("Cannot complete inst, cache data is missing: " << mem_access_info_ptr);
            return;
        }

        const InstPtr & inst_ptr = lsinfo_inst_ptr->getInstPtr();
        ILOG("Completing vector memory request " << lsinfo_inst_ptr << " for inst " << inst_ptr);
        ILOG(mem_access_info_ptr)

        // Remove from memory request buffer and schedule memory request gen event if needed
        removeFromMemoryRequestBuffer_(lsinfo_inst_ptr);

        const bool is_store_inst = inst_ptr->isStoreInst();
        if(!is_store_inst && allow_speculative_load_exec_)
        {
            removeInstFromReplayQueue_(lsinfo_inst_ptr);
        }

        VectorMemConfigPtr vector_mem_config_ptr = inst_ptr->getVectorMemConfig();
        vector_mem_config_ptr->incrementNumMemReqsCompleted();
        DLOG("Completed " << vector_mem_config_ptr->getNumMemReqsCompleted() << "/" << vector_mem_config_ptr->getNumMemReqsGenerated());
        if (vector_mem_config_ptr->getNumMemReqsGenerated() != vector_mem_config_ptr->getNumMemReqsCompleted())
        {
            return;
        }

        sparta_assert(mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT,
                      "Inst cannot finish when cache is still a miss! " << inst_ptr);
        sparta_assert(mem_access_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::HIT,
                      "Inst cannot finish when cache is still a miss! " << inst_ptr);

        ILOG("Completing vector inst: " << inst_ptr);
        inst_ptr->setStatus(Inst::Status::COMPLETED);
        lsu_insts_completed_++;
        out_lsu_credits_.send(1, 0);

        // Complete load instruction
        if (!is_store_inst)
        {
            const auto & dests = inst_ptr->getDestOpInfoList();
            sparta_assert(dests.size() == 1,
                "Load inst should have 1 dest! " << inst_ptr);
            const core_types::RegFile reg_file = olympia::coreutils::determineRegisterFile(dests[0]);
            const auto & dest_bits = inst_ptr->getDestRegisterBitMask(reg_file);
            scoreboard_views_[reg_file]->setReady(dest_bits);

            ILOG("Complete Load Instruction: " << inst_ptr->getMnemonic() << " uid("
                                               << inst_ptr->getUniqueID() << ")");
        }
        // Complete vector store instruction
        else
        {
            ILOG("Complete Store Instruction: " << inst_ptr->getMnemonic() << " uid("
                                                << inst_ptr->getUniqueID() << ")");
        }

        // NOTE:
        // Checking whether an instruction is ready to complete could be non-trivial
        // Right now we simply assume:
        // (1)Load inst is ready to complete as long as both MMU and cache access finish
        // (2)Store inst is ready to complete as long as MMU (address translation) is done
        if (isReadyToIssueInsts_())
        {
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Handle instruction flush in VLSU
    void VLSU::handleFlush_(const FlushCriteria & criteria)
    {
        LSU::handleFlush_(criteria);

        // Flush memory request ready queue and buffer
        flushMemoryRequestReadyQueue_(criteria);
        flushMemoryRequestBuffer_(criteria);
    }

    void VLSU::dumpDebugContent_(std::ostream & output) const
    {
        output << "VLSU Contents" << std::endl;
        std::cout << "Inst Queue:" << std::endl;
        for (const auto & entry : inst_queue_)
        {
            output << '\t' << entry << std::endl;
        }
        std::cout << "Memory Request Buffer:" << std::endl;
        for (const auto & entry : mem_req_buffer_)
        {
            output << '\t' << entry << "vaddr: 0x" << std::hex
                   << entry->getMemoryAccessInfoPtr()->getVAddr()
                   << std::endl;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Regular Function/Subroutine Call
    ////////////////////////////////////////////////////////////////////////////////
    void VLSU::removeFromMemoryRequestBuffer_(const LoadStoreInstInfoPtr & inst_to_remove)
    {
        ILOG("Removing memory request from the memory request buffer: " << inst_to_remove);
        sparta_assert(inst_to_remove->getMemoryRequestBufferIterator().isValid(),
            "Memory Request Buffer iterator is not valid!");
        mem_req_buffer_.erase(inst_to_remove->getMemoryRequestBufferIterator());
        // Invalidate the iterator manually
        inst_to_remove->setMemoryRequestBufferIterator(LoadStoreInstIterator());

        if (mem_req_ready_queue_.size() > 0)
        {
            uev_gen_mem_ops_.schedule(sparta::Clock::Cycle(0));
        }
    }

    bool VLSU::allOlderStoresIssued_(const InstPtr & inst_ptr)
    {
        for (const auto & ldst_info_ptr : mem_req_buffer_)
        {
            const auto & ldst_inst_ptr = ldst_info_ptr->getInstPtr();
            const auto & mem_info_ptr = ldst_info_ptr->getMemoryAccessInfoPtr();
            if (ldst_inst_ptr->isStoreInst()
                && ldst_inst_ptr->getUniqueID() < inst_ptr->getUniqueID()
                && !mem_info_ptr->getPAddrStatus() && ldst_info_ptr->getInstPtr() != inst_ptr
                && ldst_inst_ptr->getUOpID() < inst_ptr->getUOpID())
            {
                return false;
            }
        }
        return true;
    }

    void VLSU::flushMemoryRequestReadyQueue_(const FlushCriteria & criteria)
    {
        // TODO: Replace with erase_if with c++20
        auto iter = ready_queue_.begin();
        while (iter != ready_queue_.end())
        {
            auto inst_ptr = (*iter)->getInstPtr();
            if (criteria.includedInFlush(inst_ptr))
            {
                DLOG("Flushing from ready queue - Instruction ID: " << inst_ptr->getUniqueID());
                ready_queue_.erase(++iter);
            }
        }
    }

    void VLSU::flushMemoryRequestBuffer_(const FlushCriteria & criteria)
    {
        // TODO: Replace with erase_if with c++20
        auto iter = mem_req_buffer_.begin();
        while (iter != mem_req_buffer_.end())
        {
            auto inst_ptr = (*iter)->getInstPtr();
            if (criteria.includedInFlush(inst_ptr))
            {
                DLOG("Flushing from memory request buffer: " << *iter);
                mem_req_buffer_.erase(++iter);
            }
        }
    }

    /*
    // Update issue priority when newly dispatched instruction comes in
    void VLSU::updateIssuePriorityAfterNewDispatch_(
        const LoadStoreInstInfoPtr & load_store_lsinfo_inst_ptr)
    {
        for (auto & lsinfo_inst_ptr : mem_req_buffer_)
        {
            if (lsinfo_inst_ptr->getMemoryAccessInfoPtr()->getVAddr()
                    == load_store_lsinfo_inst_ptr->getMemoryAccessInfoPtr()->getVAddr()
                && lsinfo_inst_ptr->getInstPtr() == load_store_lsinfo_inst_ptr->getInstPtr())
            {
                lsinfo_inst_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                lsinfo_inst_ptr->setPriority(LoadStoreInstInfo::IssuePriority::NEW_DISP);
                // NOTE:
                // IssuePriority should always be updated before a new issue event is scheduled.
                // This guarantees that whenever a new instruction issue event is scheduled:
                // (1)Instruction issue queue already has "something READY";
                // (2)Instruction issue arbitration is guaranteed to be sucessful.
                // Update instruction status
                lsinfo_inst_ptr->setVLSUStatusState(Inst::Status::SCHEDULED);
                if (lsinfo_inst_ptr->getInstPtr()->getStatus() != Inst::Status::SCHEDULED)
                {
                    lsinfo_inst_ptr->getInstPtr()->setStatus(Inst::Status::SCHEDULED);
                }
                return;
            }
        }

        sparta_assert(false,
            "Attempt to update issue priority for instruction not yet in the issue queue!");
    }

    // Update issue priority after tlb reload
    void VLSU::updateIssuePriorityAfterTLBReload_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        bool is_found = false;
        for (auto & lsinfo_inst_ptr : mem_req_buffer_)
        {
            const MemoryAccessInfoPtr & mem_info_ptr = lsinfo_inst_ptr->getMemoryAccessInfoPtr();
            if (mem_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::MISS)
            {
                // Re-activate all TLB-miss-pending instructions in the issue queue
                if (!allow_speculative_load_exec_) // Speculative misses are marked as not ready and
                                                   // replay event would set them back to ready
                {
                    lsinfo_inst_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                }
                lsinfo_inst_ptr->setPriority(LoadStoreInstInfo::IssuePriority::MMU_PENDING);
            }
            // NOTE:
            // We may not have to re-activate all of the pending MMU miss instruction here
            // However, re-activation must be scheduled somewhere else

            if (lsinfo_inst_ptr->getInstPtr() == inst_ptr)
            {
                // Update issue priority for this outstanding TLB miss
                if (lsinfo_inst_ptr->getState() != LoadStoreInstInfo::IssueState::ISSUED)
                {
                    lsinfo_inst_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                }
                lsinfo_inst_ptr->setPriority(LoadStoreInstInfo::IssuePriority::MMU_RELOAD);
                uev_append_ready_.preparePayload(lsinfo_inst_ptr)->schedule(sparta::Clock::Cycle(0));

                // NOTE:
                // The priority should be set in such a way that
                // the outstanding miss is always re-issued earlier than other pending miss
                // Here we have MMU_RELOAD > MMU_PENDING

                is_found = true;
            }
        }

        sparta_assert(inst_ptr->getFlushedStatus() || is_found,
                      "Attempt to rehandle TLB lookup for instruction not yet in the issue queue! "
                          << inst_ptr);
    }

    // Update issue priority after cache reload
    void VLSU::updateIssuePriorityAfterCacheReload_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();

        sparta_assert(inst_ptr->getFlushedStatus() == false,
                      "Attempt to rehandle cache lookup for flushed instruction!");

        const LoadStoreInstIterator & iter = mem_access_info_ptr->getIssueQueueIterator();
        sparta_assert(
            iter.isValid(),
            "Attempt to rehandle cache lookup for instruction not yet in the issue queue! "
                << mem_access_info_ptr);

        const LoadStoreInstInfoPtr & lsinfo_inst_ptr = *(iter);

        // Update issue priority for this outstanding cache miss
        if (lsinfo_inst_ptr->getState() != LoadStoreInstInfo::IssueState::ISSUED)
        {
            lsinfo_inst_ptr->setState(LoadStoreInstInfo::IssueState::READY);
        }
        lsinfo_inst_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_RELOAD);
        uev_append_ready_.preparePayload(lsinfo_inst_ptr)->schedule(sparta::Clock::Cycle(0));
    }

    // Update issue priority after store instruction retires
    void VLSU::updateIssuePriorityAfterStoreInstRetire_(const LoadStoreInstInfoPtr & inst_ptr)
    {
        if (inst_ptr->getInstPtr()->isVector())
        {
            for (auto & lsinfo_inst_ptr : mem_req_buffer_)
            {
                if (lsinfo_inst_ptr->getMemoryAccessInfoPtr()->getVAddr()
                    == inst_ptr->getMemoryAccessInfoPtr()->getVAddr())
                {

                    if (lsinfo_inst_ptr->getState()
                        != LoadStoreInstInfo::IssueState::ISSUED) // Speculative misses are marked
                                                                  // as not ready and replay event
                                                                  // would set them back to ready
                    {
                        lsinfo_inst_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                    }
                    lsinfo_inst_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_PENDING);
                    uev_append_ready_.preparePayload(lsinfo_inst_ptr)
                        ->schedule(sparta::Clock::Cycle(0));

                    return;
                }
            }

            sparta_assert(false,
                "Attempt to update issue priority for instruction not yet in the issue queue!");
        }
    }
    */
} // namespace olympia
