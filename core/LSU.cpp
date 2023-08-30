#include "sparta/utils/SpartaAssert.hpp"
#include "CoreUtils.hpp"
#include "LSU.hpp"

namespace olympia
{
    const char LSU::name[] = "lsu";

    ////////////////////////////////////////////////////////////////////////////////
    // Constructor
    ////////////////////////////////////////////////////////////////////////////////

    LSU::LSU(sparta::TreeNode *node, const LSUParameterSet *p) :
        sparta::Unit(node),
        memory_access_allocator(50, 30),   // 50 and 30 are arbitrary numbers here.  It can be tuned to an exact value.
        load_store_info_allocator(50, 30),
        ldst_inst_queue_("lsu_inst_queue", p->ldst_inst_queue_size, getClock()),
        ldst_inst_queue_size_(p->ldst_inst_queue_size),
        load_queue_("load_queue", p->ld_queue_size, getClock()),
        load_queue_size_(p->ld_queue_size),
        store_queue_("store_queue", p->st_queue_size, getClock()),
        store_queue_size_(p->st_queue_size),
        replay_buffer_("replay_buffer", p->st_queue_size, getClock()),
        replay_buffer_size_(p->replay_buffer_size),
        stall_pipeline_on_miss_(p->stall_pipeline_on_miss),
        allow_speculative_load_exec_(p->allow_speculative_load_exec)
    {
        // Pipeline collection config
        ldst_pipeline_.enableCollection(node);
        ldst_inst_queue_.enableCollection(node);

        load_queue_.enableCollection(node);
        store_queue_.enableCollection(node);
        replay_buffer_.enableCollection(node);

        // Startup handler for sending initial credits
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(LSU, sendInitialCredits_));


        // Port config
        in_lsu_insts_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getInstsFromDispatch_, InstPtr));

        in_rob_retire_ack_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getAckFromROB_, InstPtr));

        in_reorder_flush_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, handleFlush_, FlushManager::FlushingCriteria));

        in_mmu_lookup_req_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getInstFromMMU_, MemoryAccessInfoPtr));

        in_mmu_lookup_ack_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getAckFromMMU_, MemoryAccessInfoPtr));

        in_cache_lookup_req_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getInstFromCache_, MemoryAccessInfoPtr));

        in_cache_lookup_ack_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getAckFromCache_, MemoryAccessInfoPtr));
        // Allow the pipeline to create events and schedule work
        ldst_pipeline_.performOwnUpdates();

        // There can be situations where NOTHING is going on in the
        // simulator but forward progression of the pipeline elements.
        // In this case, the internal event for the LS pipeline will
        // be the only event keeping simulation alive.  Sparta
        // supports identifying non-essential events (by calling
        // setContinuing to false on any event).
        ldst_pipeline_.setContinuing(true);

        ldst_pipeline_.registerHandlerAtStage<sparta::SchedulingPhase::Tick>(static_cast<uint32_t>(PipelineStage::MMU_LOOKUP),
                                                CREATE_SPARTA_HANDLER(LSU, handleMMULookupReq1_));

        ldst_pipeline_.registerHandlerAtStage<sparta::SchedulingPhase::Tick>(static_cast<uint32_t>(PipelineStage::MMU_LOOKUP),
                                              CREATE_SPARTA_HANDLER(LSU, handleMMULookupReq2_));

        ldst_pipeline_.registerHandlerAtStage<sparta::SchedulingPhase::Tick>(static_cast<uint32_t>(PipelineStage::CACHE_LOOKUP),
                                              CREATE_SPARTA_HANDLER(LSU, handleCacheLookupReq1_));

        ldst_pipeline_.registerHandlerAtStage<sparta::SchedulingPhase::Tick>(static_cast<uint32_t>(PipelineStage::CACHE_LOOKUP),
                                              CREATE_SPARTA_HANDLER(LSU, handleCacheLookupReq2_));

        ldst_pipeline_.registerHandlerAtStage(static_cast<uint32_t>(PipelineStage::COMPLETE),
                                                CREATE_SPARTA_HANDLER(LSU, completeInst_));

        // NOTE:
        // To resolve the race condition when:
        // Both cache and MMU try to drive the single BIU port at the same cycle
        // Here we give cache the higher priority
        ILOG("LSU construct: #" << node->getGroupIdx());

    }


    ////////////////////////////////////////////////////////////////////////////////
    // Callbacks
    ////////////////////////////////////////////////////////////////////////////////

    // Send initial credits (ldst_inst_queue_size_) to Dispatch Unit
    void LSU::sendInitialCredits_()
    {
        setupScoreboard_();
        out_lsu_credits_.send(ldst_inst_queue_size_);

        ILOG("LSU initial credits for Dispatch Unit: " << ldst_inst_queue_size_);
    }

    // Setup scoreboard View
    void LSU::setupScoreboard_() {
        // Setup scoreboard view upon register file
        std::vector<core_types::RegFile> reg_files = {core_types::RF_INTEGER, core_types::RF_FLOAT};
        for(const auto rf : reg_files)
        {
            scoreboard_views_[rf].reset(new sparta::ScoreboardView(getContainer()->getName(),
                                                                   core_types::regfile_names[rf],
                                                                   getContainer()));
        }
    }

    // Receive new load/store instruction from Dispatch Unit
    void LSU::getInstsFromDispatch_(const InstPtr &inst_ptr)
    {
        bool all_ready = true; // assume all ready
        // address operand check
        if (!scoreboard_views_[core_types::RF_INTEGER]->isSet(inst_ptr->getSrcRegisterBitMask(core_types::RF_INTEGER))) {
            all_ready = false;
            const auto &src_bits = inst_ptr->getSrcRegisterBitMask(core_types::RF_INTEGER);
            scoreboard_views_[core_types::RF_INTEGER]->registerReadyCallback(src_bits, inst_ptr->getUniqueID(),
                                                                             [this, inst_ptr](const sparta::Scoreboard::RegisterBitMask &)
                                                                             {
                                                                                 this->getInstsFromDispatch_(inst_ptr);
                                                                             });
            ILOG("Instruction NOT ready: " << inst_ptr << " Bits needed:" << sparta::printBitSet(src_bits));
        }
        else {
            // we wait for address operand to be ready before checking data operand in the case of stores
            // this way we avoid two live callbacks
            if (inst_ptr->isStoreInst()) {
                const auto rf = inst_ptr->getRenameData().getDataReg().rf;
                const auto &data_bits = inst_ptr->getDataRegisterBitMask(rf);

                if (!scoreboard_views_[rf]->isSet(data_bits)) {
                    all_ready = false;
                    scoreboard_views_[rf]->registerReadyCallback(data_bits, inst_ptr->getUniqueID(),
                                                                [this, inst_ptr](const sparta::Scoreboard::RegisterBitMask &)
                                                                {
                                                                    this->getInstsFromDispatch_(inst_ptr);
                                                                });
                    ILOG("Instruction NOT ready: " << inst_ptr << " Bits needed:" << sparta::printBitSet(data_bits));
                }
            }
        }

        if (all_ready) {
            // Create load/store memory access info
            MemoryAccessInfoPtr mem_info_ptr = sparta::allocate_sparta_shared_pointer<MemoryAccessInfo>(memory_access_allocator,
                                                                                                        inst_ptr);

            // Create load/store instruction issue info
            LoadStoreInstInfoPtr inst_info_ptr = sparta::allocate_sparta_shared_pointer<LoadStoreInstInfo>(load_store_info_allocator,
                                                                                                        mem_info_ptr);
            lsu_insts_dispatched_++;

            // Append to instruction issue queue
            appendIssueQueue_(inst_info_ptr);

            if(inst_ptr->isStoreInst()){
                appendStoreQueue_(inst_info_ptr);
            }else {
                appendLoadQueue_(inst_info_ptr);
            }

            // Update issue priority & Schedule an instruction issue event
            updateIssuePriorityAfterNewDispatch_(inst_ptr);
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));

            // NOTE:
            // IssuePriority should always be updated before a new issue event is scheduled.
            // This guarantees that whenever a new instruction issue event is scheduled:
            // (1)Instruction issue queue already has "something READY";
            // (2)Instruction issue arbitration is guaranteed to be sucessful.

            // Update instruction status
            inst_ptr->setStatus(Inst::Status::SCHEDULED);

            // NOTE:
            // It is a bug if instruction status is updated as SCHEDULED in the issueInst_()
            // The reason is: when issueInst_() is called, it could be scheduled for
            // either a new issue event, or a re-issue event
            // however, we can ONLY update instruction status as SCHEDULED for a new issue event

            ILOG("Another issue event scheduled " << inst_ptr);
        }
    }

    // Receive update from ROB whenever store instructions retire
    void LSU::getAckFromROB_(const InstPtr & inst_ptr)
    {
        sparta_assert(inst_ptr->getStatus() == Inst::Status::RETIRED,
                        "Get ROB Ack, but the store inst hasn't retired yet!");

        stores_retired_++;

        updateIssuePriorityAfterStoreInstRetire_(inst_ptr);
        uev_issue_inst_.schedule(sparta::Clock::Cycle(0));


        ILOG("Get Ack from ROB! Retired store instruction: " << inst_ptr);
    }

    // Issue/Re-issue ready instructions in the issue queue
    void LSU::issueInst_()
    {
        if(ldst_pipeline_.isStalledOrStalling() && stall_pipeline_on_miss_){
            uev_issue_inst_.schedule(sparta::Clock::Cycle(1));
            return;
        }
        // Instruction issue arbitration
        const LoadStoreInstInfoPtr & win_ptr = arbitrateInstIssue_();
        // NOTE:
        // win_ptr should always point to an instruction ready to be issued
        // Otherwise assertion error should already be fired in arbitrateInstIssue_()

        lsu_insts_issued_++;

        // Append load/store pipe
        ldst_pipeline_.append(win_ptr);

        // Update instruction issue info
        win_ptr->setState(LoadStoreInstInfo::IssueState::ISSUED);
        win_ptr->setPriority(LoadStoreInstInfo::IssuePriority::LOWEST);

        // Schedule another instruction issue event if possible
        if (isReadyToIssueInsts_() && !stall_pipeline_on_miss_) {
            uev_issue_inst_.schedule(sparta::Clock::Cycle(1));
        }

        ILOG("Issue/Re-issue Instruction: " << win_ptr->getInstPtr());
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Cache Subroutine
    ////////////////////////////////////////////////////////////////////////////////
    // Handle cache access request
    void LSU::handleCacheLookupReq1_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::CACHE_LOOKUP);

        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(stage_id)) {
            return;
        }

        const LoadStoreInstInfoPtr &load_store_info_ptr = ldst_pipeline_[stage_id];
        const MemoryAccessInfoPtr & mem_access_info_ptr = load_store_info_ptr->getMemoryAccessInfoPtr();
        const InstPtr &inst_ptr = mem_access_info_ptr->getInstPtr();
        ILOG(mem_access_info_ptr);

        const bool phy_addr_is_ready =
                mem_access_info_ptr->getPhyAddrStatus();
        const bool is_already_hit =
                (mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT);
        const bool is_unretired_store =
                inst_ptr->isStoreInst() && (inst_ptr->getStatus() != Inst::Status::RETIRED);
        const bool cache_bypass = is_already_hit || !phy_addr_is_ready || is_unretired_store;

        if (cache_bypass) {
            if (is_already_hit) {
                ILOG("Cache Lookup is skipped (Cache already hit)!");
            }
            else if (!phy_addr_is_ready) {
                ILOG("Cache Lookup is skipped (Physical address not ready)!");
            }
            else if (is_unretired_store) {
                ILOG("Cache Lookup is skipped (Un-retired store instruction)!");
            }
            else {
                sparta_assert(false, "Cache access is bypassed without a valid reason!");
            }
            cache_hit_ = true;
            return;
        }

        cache_hit_ = false;
        out_cache_lookup_req_.send(mem_access_info_ptr);
    }

    void LSU::handleCacheLookupReq2_(){
        auto cache_lookup_stage_id = static_cast<uint32_t>(PipelineStage::CACHE_LOOKUP);

        if(!cache_hit_) {
            if (stall_pipeline_on_miss_) {
                uev_pipe_stall_.schedule(sparta::Clock::Cycle(0));
            } else {
                ldst_pipeline_.invalidateStage(cache_lookup_stage_id);
            }
        }
    }

    void LSU::getInstFromCache_(const MemoryAccessInfoPtr &memory_access_info_ptr){
        auto & inst_ptr = memory_access_info_ptr->getInstPtr();
        if (cache_pending_inst_flushed_) {
            cache_pending_inst_flushed_ = false;
            ILOG("BIU Ack for a flushed cache miss is received!");

            // Schedule an instruction (re-)issue event
            // Note: some younger load/store instruction(s) might have been blocked by
            // this outstanding miss
            updateIssuePriorityAfterCacheReload_(inst_ptr, true);
            if (isReadyToIssueInsts_()) {
                uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
            }

            return;
        }
        if(stall_pipeline_on_miss_) {
            if(!cache_hit_) {
                out_cache_lookup_req_.send(memory_access_info_ptr, 0);
            }
        } else {
            updateIssuePriorityAfterCacheReload_(inst_ptr);
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    void LSU::getAckFromCache_(const MemoryAccessInfoPtr &updated_memory_access_info_ptr){
        cache_hit_ = updated_memory_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT;
    }

    // Retire load/store instruction
    void LSU::completeInst_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::COMPLETE);

        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(stage_id)) {
            return;
        }


        const LoadStoreInstInfoPtr &load_store_info_ptr = ldst_pipeline_[stage_id];
        const MemoryAccessInfoPtr & mem_access_info_ptr = load_store_info_ptr->getMemoryAccessInfoPtr();
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        bool is_store_inst = inst_ptr->isStoreInst();
        ILOG("Completing inst: " << inst_ptr);
        ILOG(mem_access_info_ptr);

        core_types::RegFile reg_file = core_types::RF_INTEGER;
        const auto & dests = inst_ptr->getDestOpInfoList();
        if(dests.size() > 0){
            sparta_assert(dests.size() == 1); // we should only have one destination
            reg_file = olympia::coreutils::determineRegisterFile(dests[0]);
            const auto & dest_bits = inst_ptr->getDestRegisterBitMask(reg_file);
            scoreboard_views_[reg_file]->setReady(dest_bits);
        }

        // Complete load instruction
        if (!is_store_inst) {
            sparta_assert(mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT,
                        "Load instruction cannot complete when cache is still a miss!");

            // Update instruction status
            inst_ptr->setStatus(Inst::Status::COMPLETED);

            lsu_insts_completed_++;

            // Remove completed instruction from issue queue
            popIssueQueue_(load_store_info_ptr);

            popLoadQueue_(load_store_info_ptr);

            // Update instruction issue queue credits to Dispatch Unit
            out_lsu_credits_.send(1, 0);

            ILOG("Complete Load Instruction: "
                 << inst_ptr->getMnemonic()
                 << " uid(" << inst_ptr->getUniqueID() << ")");

            return;
        }


        // Complete store instruction
        if (inst_ptr->getStatus() != Inst::Status::RETIRED) {

            sparta_assert(mem_access_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::HIT,
                        "Store instruction cannot complete when TLB is still a miss!");

            // Update instruction status
            inst_ptr->setStatus(Inst::Status::COMPLETED);


            ILOG("Complete Store Instruction: "
                 << inst_ptr->getMnemonic()
                 << " uid(" << inst_ptr->getUniqueID() << ")");
        }
        // Finish store operation
        else {
            sparta_assert(mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT,
                        "Store inst cannot finish when cache is still a miss!");

            sparta_assert(mem_access_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::HIT,
                          "Store inst cannot finish when cache is still a miss!");

            lsu_insts_completed_++;

            // Remove store instruction from issue queue
            popIssueQueue_(load_store_info_ptr);

            popStoreQueue_(load_store_info_ptr);

            // Update instruction issue queue credits to Dispatch Unit
            out_lsu_credits_.send(1, 0);

            ILOG("Store operation is done!");
        }


        // NOTE:
        // Checking whether an instruction is ready to complete could be non-trivial
        // Right now we simply assume:
        // (1)Load inst is ready to complete as long as both MMU and cache access finish
        // (2)Store inst is ready to complete as long as MMU (address translation) is done
    }

    // Handle instruction flush in LSU
    void LSU::handleFlush_(const FlushCriteria & criteria)
    {
        ILOG("Start Flushing!");

        // Flush criteria setup
        auto flush = [criteria] (const uint64_t & id) -> bool {
            return id >= static_cast<uint64_t>(criteria);
        };

        lsu_flushes_++;

        // Flush load/store pipeline entry
        flushLSPipeline_(flush);

        // Mark flushed flag for unfinished speculative MMU access
        if (mmu_busy_) {
            mmu_pending_inst_flushed = true;
        }

        // Mark flushed flag for unfinished speculative cache access
        if (cache_busy_) {
            cache_pending_inst_flushed_ = true;
        }

        // Flush instruction issue queue
        flushIssueQueue_(flush);

        // Cancel issue event already scheduled if no ready-to-issue inst left after flush
        if (!isReadyToIssueInsts_()) {
            uev_issue_inst_.cancel();
        }

        // NOTE:
        // Flush is handled at Flush phase (inbetween PortUpdate phase and Tick phase).
        // This also guarantees that whenever an instruction issue event happens,
        // instruction issue arbitration should always succeed, even when flush happens.
        // Otherwise, assertion error is fired inside arbitrateInstIssue_()
    }


    ////////////////////////////////////////////////////////////////////////////////
    // Regular Function/Subroutine Call
    ////////////////////////////////////////////////////////////////////////////////

    // Append new load/store instruction into issue queue
    void LSU::appendIssueQueue_(const LoadStoreInstInfoPtr & inst_info_ptr)
    {
        sparta_assert(ldst_inst_queue_.size() < ldst_inst_queue_size_,
                        "Appending issue queue causes overflows!");

        // Always append newly dispatched instructions to the back of issue queue
        uint32_t index = ldst_inst_queue_.size();
        ldst_inst_queue_.push_back(inst_info_ptr);
        inst_info_ptr->setIssueQueueIndex(index);

        ILOG("Append new load/store instruction to issue queue!");
    }

    void LSU::appendLoadQueue_(const LoadStoreInstInfoPtr &inst_info_ptr) {
        appendToQueue_(inst_info_ptr, load_queue_);
    }

    void LSU::appendStoreQueue_(const LoadStoreInstInfoPtr &inst_info_ptr) {
        appendToQueue_(inst_info_ptr, store_queue_);
    }

    void inline LSU::appendToQueue_(const LoadStoreInstInfoPtr &inst_info_ptr, LoadStoreIssueQueue &queue){
        sparta_assert(queue.size() < queue.capacity(),
                      "Appending load queue causes overflows!");

        // Always append newly dispatched instructions to the back of issue queue
        uint32_t index = queue.size();
        queue.push_back(inst_info_ptr);
        inst_info_ptr->setQueueIndex(index);

        ILOG("Append new instruction to queue!");
    }

    // Pop completed store instruction out of store queue
    void LSU::popStoreQueue_(const LoadStoreInstInfoPtr & inst_info_ptr){
        removeFromQueue_(inst_info_ptr, store_queue_);
    }

    void LSU::popLoadQueue_(const LoadStoreInstInfoPtr & inst_info_ptr){
        removeFromQueue_(inst_info_ptr, load_queue_);
    }

    void LSU::popIssueQueue_(const LoadStoreInstInfoPtr & inst_info_ptr){
        for (auto iter = ldst_inst_queue_.begin(); iter != ldst_inst_queue_.end(); iter++) {
            if (*iter == inst_info_ptr) {
                ldst_inst_queue_.erase(iter);

                return;
            }
        }

        sparta_assert(false, "Attempt to complete instruction no longer existing in the queue!");
    }

    void inline LSU::removeFromQueue_(const LoadStoreInstInfoPtr &inst_info_ptr, LoadStoreIssueQueue &queue){
        for (auto iter = queue.begin(); iter != queue.end(); iter++) {
            if (*iter == inst_info_ptr) {
                queue.erase(iter);

                return;
            }
        }

        sparta_assert(false, "Attempt to complete instruction no longer existing in the queue!");
    }

    // Arbitrate instruction issue from ldst_inst_queue
    const LSU::LoadStoreInstInfoPtr & LSU::arbitrateInstIssue_()
    {

        sparta_assert(ldst_inst_queue_.size() > 0, "Arbitration fails: issue is empty!");

        // Initialization of winner
        auto win_ptr_iter = ldst_inst_queue_.begin();

        // Select the ready instruction with highest issue priority
        for (auto iter = ldst_inst_queue_.begin(); iter != ldst_inst_queue_.end(); iter++) {
            // Skip not ready-to-issue instruction
            if (!(*iter)->isReady()) {
                continue;
            }

            if (stall_pipeline_on_miss_) {
                if((*iter)->getPriority() == LoadStoreInstInfo::IssuePriority::NEW_DISP) {
                    return *iter;
                }
            } else{
                // Pick winner
                if (!(*win_ptr_iter)->isReady() || (*iter)->winArb(*win_ptr_iter)) {
                    win_ptr_iter = iter;
                }
            }
            // NOTE:
            // If the inst pointed to by (*win_ptr_iter) is not ready (possible @initialization),
            // Re-assign it pointing to the ready-to-issue instruction pointed by (*iter).
            // Otherwise, both (*win_ptr_iter) and (*iter) point to ready-to-issue instructions,
            // Pick the one with higher issue priority.
        }

        sparta_assert((*win_ptr_iter)->isReady(), "Arbitration fails: no instruction is ready!");

        return *win_ptr_iter;

    }

    // Check for ready to issue instructions
    bool LSU::isReadyToIssueInsts_() const
    {
        // Check if there is at least one ready-to-issue instruction in issue queue
        for (auto const &inst_info_ptr : ldst_inst_queue_) {
            if (inst_info_ptr->isReady()) {
                ILOG("At least one instruction is ready to be issued: " << inst_info_ptr);
                return true;
            }
        }

        ILOG("No instructions are ready to be issued");

        return false;
    }

    // Update issue priority when newly dispatched instruction comes in
    void LSU::updateIssuePriorityAfterNewDispatch_(const InstPtr & inst_ptr)
    {
        for (auto &inst_info_ptr : ldst_inst_queue_) {
            if (inst_info_ptr->getInstPtr() == inst_ptr) {

                inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::NEW_DISP);

                return;
            }
        }

        sparta_assert(false,
            "Attempt to update issue priority for instruction not yet in the issue queue!");
    }

    // Update issue priority after cache reload
    void LSU::updateIssuePriorityAfterTLBReload_(const InstPtr & inst_ptr,
                                                 const bool is_flushed_inst)
    {
        bool is_found = false;

        for (auto &inst_info_ptr : ldst_inst_queue_) {
            const MemoryAccessInfoPtr & mem_info_ptr = inst_info_ptr->getMemoryAccessInfoPtr();

            if (mem_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::MISS) {
                // Re-activate all TLB-miss-pending instructions in the issue queue
                inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::MMU_PENDING);

                // NOTE:
                // We may not have to re-activate all of the pending MMU miss instruction here
                // However, re-activation must be scheduled somewhere else

                if (inst_info_ptr->getInstPtr() == inst_ptr) {
                    // Update issue priority for this outstanding TLB miss
                    inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                    inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::MMU_RELOAD);

                    // NOTE:
                    // The priority should be set in such a way that
                    // the outstanding miss is always re-issued earlier than other pending miss
                    // Here we have MMU_RELOAD > MMU_PENDING

                    is_found = true;
                }
            }
        }

        sparta_assert(is_flushed_inst || is_found,
            "Attempt to rehandle TLB lookup for instruction not yet in the issue queue!");
    }

    // Update issue priority after cache reload
    void LSU::updateIssuePriorityAfterCacheReload_(const InstPtr & inst_ptr,
                                                   const bool is_flushed_inst)
    {
        bool is_found = false;

        for (auto &inst_info_ptr : ldst_inst_queue_) {
            const MemoryAccessInfoPtr & mem_info_ptr = inst_info_ptr->getMemoryAccessInfoPtr();

            if (mem_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::MISS) {
                // Re-activate all cache-miss-pending instructions in the issue queue
                inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_PENDING);

                // NOTE:
                // We may not have to re-activate all of the pending cache miss instruction here
                // However, re-activation must be scheduled somewhere else

                if (inst_info_ptr->getInstPtr() == inst_ptr) {
                    // Update issue priority for this outstanding cache miss
                    inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                    inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_RELOAD);

                    // NOTE:
                    // The priority should be set in such a way that
                    // the outstanding miss is always re-issued earlier than other pending miss
                    // Here we have CACHE_RELOAD > CACHE_PENDING > MMU_RELOAD

                    is_found = true;
                }
            }
        }

        sparta_assert(is_flushed_inst || is_found,
                    "Attempt to rehandle cache lookup for instruction not yet in the issue queue!");
    }

    // Update issue priority after store instruction retires
    void LSU::updateIssuePriorityAfterStoreInstRetire_(const InstPtr & inst_ptr)
    {
        for (auto &inst_info_ptr : ldst_inst_queue_) {
            if (inst_info_ptr->getInstPtr() == inst_ptr) {

                inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_PENDING);

                return;
            }
        }

        sparta_assert(false,
            "Attempt to update issue priority for instruction not yet in the issue queue!");

    }

    ////////////////////////////////////////////////////////////////////////////////
    // MMU subroutines
    ////////////////////////////////////////////////////////////////////////////////
    // Handle MMU access request
    void LSU::handleMMULookupReq1_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::MMU_LOOKUP);

        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(stage_id)) {
            return;
        }

        const LoadStoreInstInfoPtr &load_store_info_ptr = ldst_pipeline_[stage_id];
        const MemoryAccessInfoPtr & mem_access_info_ptr = load_store_info_ptr->getMemoryAccessInfoPtr();
        ILOG(mem_access_info_ptr);

        bool mmu_bypass = (mem_access_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::HIT);

        if (mmu_bypass) {
            ILOG("MMU Lookup is skipped (TLB is already hit)!");
            mmu_hit_ = true;
            return;
        }

        mmu_hit_ = false;
        out_mmu_lookup_req_.send(mem_access_info_ptr);
    }

    // Handle MMU access request
    void LSU::handleMMULookupReq2_()
    {
        auto stage_id = static_cast<uint32_t>(PipelineStage::MMU_LOOKUP);

        // If miss
        if (!mmu_hit_) {
            if (stall_pipeline_on_miss_) {
                uev_pipe_stall_.schedule(sparta::Clock::Cycle(0));
            } else {
                ldst_pipeline_.invalidateStage(stage_id);
            }
        }
    }

    // MMU ready with information
    void LSU::getInstFromMMU_(const MemoryAccessInfoPtr &memory_access_info_ptr) {
        const auto &inst_ptr = memory_access_info_ptr->getInstPtr();
        if (mmu_pending_inst_flushed) {
            mmu_pending_inst_flushed = false;
            // Update issue priority & Schedule an instruction (re-)issue event
            updateIssuePriorityAfterTLBReload_(inst_ptr, true);
            if (isReadyToIssueInsts_()) {
                uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
            }
            return;
        }
        if(stall_pipeline_on_miss_) {
            if(!mmu_hit_){
                out_mmu_lookup_req_.send(memory_access_info_ptr, 0);
            }
        }else{
            updateIssuePriorityAfterTLBReload_(inst_ptr);
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }

        ILOG("MMU rehandling event is scheduled!");
    }

    void LSU::getAckFromMMU_(const MemoryAccessInfoPtr &updated_memory_access_info_ptr) {
        mmu_hit_ = updated_memory_access_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::HIT;

        if(mmu_hit_) {
//            if(updated_memory_access_info_ptr->getInstPtr()->isStoreInst() && allow_speculative_load_exec_){
//                reIssueMatchingYoungerLoads(updated_memory_access_info_ptr);
//            }else if(!updated_memory_access_info_ptr->getInstPtr()->isStoreInst()){// TODO Update stores with matching load address
//                updateMatchingStoresWithLoad(updated_memory_access_info_ptr);
//            }
        }
    }

    void LSU::pipeStall_() {
        auto mmu_stage_id = static_cast<uint32_t>(PipelineStage::MMU_LOOKUP);
        auto cache_stage_id = static_cast<uint32_t>(PipelineStage::CACHE_LOOKUP);

        if (ldst_pipeline_.isValid(cache_stage_id)) {
            const MemoryAccessInfoPtr & cache_mem_access_info_ptr = ldst_pipeline_[cache_stage_id]->getMemoryAccessInfoPtr();
            if(!cache_mem_access_info_ptr->getDataIsReady()){
                ldst_pipeline_.stall(cache_stage_id, 1);
                uev_pipe_stall_.schedule(sparta::Clock::Cycle(1));
            }
        }else if (ldst_pipeline_.isValid(mmu_stage_id)) {
            const MemoryAccessInfoPtr & mmu_mem_access_info_ptr = ldst_pipeline_[mmu_stage_id]->getMemoryAccessInfoPtr();
            if(!mmu_mem_access_info_ptr->getPhyAddrIsReady()){
                ldst_pipeline_.stall(mmu_stage_id, 1);
                uev_pipe_stall_.schedule(sparta::Clock::Cycle(1));
            }
        }
    }

    void LSU::reIssueMatchingYoungerLoads(const MemoryAccessInfoPtr &mem_access_info_ptr) {
        const auto match_inst_ptr = mem_access_info_ptr->getInstPtr();
        for (auto const &inst_info_ptr: load_queue_) {
            if (!inst_info_ptr->isReady()) continue;
            auto load_inst_ptr = inst_info_ptr->getInstPtr();

            if (load_inst_ptr->getUniqueID() > match_inst_ptr->getUniqueID() &&
                    load_inst_ptr->getTargetVAddr() == match_inst_ptr->getTargetVAddr()) {
                inst_info_ptr->reIssue();
            }
        }
    }

    void LSU::updateMatchingStoresWithLoad(const MemoryAccessInfoPtr &mem_access_info_ptr) {
        const auto match_inst_ptr = mem_access_info_ptr->getInstPtr();
        for (auto const &inst_info_ptr: store_queue_) {
            if(!inst_info_ptr->isReady()) continue;
            auto store_inst_ptr = inst_info_ptr->getInstPtr();

            if(store_inst_ptr->getTargetVAddr() == match_inst_ptr->getTargetVAddr()) {
                inst_info_ptr->setHasDependentInst(true);
            }
        }
    }
    // Flush instruction issue queue
    template<typename Comp>
    void LSU::flushIssueQueue_(const Comp & flush)
    {
        uint32_t credits_to_send = 0;

        auto iter = ldst_inst_queue_.begin();
        while (iter != ldst_inst_queue_.end()) {
            auto inst_id = (*iter)->getInstPtr()->getUniqueID();

            auto delete_iter = iter++;

            if (flush(inst_id)) {
                ldst_inst_queue_.erase(delete_iter);

                // NOTE:
                // We cannot increment iter after erase because it's already invalidated by then

                ++credits_to_send;

                ILOG("Flush Instruction ID: " << inst_id);
            }
        }

        if (credits_to_send > 0) {
            out_lsu_credits_.send(credits_to_send);

            ILOG("Flush " << credits_to_send << " instructions in issue queue!");
        }
    }

    // Flush load/store pipe
    template<typename Comp>
    void LSU::flushLSPipeline_(const Comp & flush)
    {
        uint32_t stage_id = 0;
        for (auto iter = ldst_pipeline_.begin(); iter != ldst_pipeline_.end(); iter++, stage_id++) {
            // If the pipe stage is already invalid, no need to flush
            if (!iter.isValid()) {
                continue;
            }

            auto inst_id = (*iter)->getInstPtr()->getUniqueID();
            if (flush(inst_id)) {
                ldst_pipeline_.flushStage(iter);

                ILOG("Flush Pipeline Stage[" << stage_id
                     << "], Instruction ID: " << inst_id);
            }
        }
    }

} // namespace olympia