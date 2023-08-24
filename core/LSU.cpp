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
        ld_queue_size_(p->ld_queue_size),
        store_queue_("store_queue", p->st_queue_size, getClock()),
        st_queue_size_(p->st_queue_size),

        dl1_always_hit_(p->dl1_always_hit),
        stall_pipeline_on_miss_(p->stall_pipeline_on_miss),
        allow_speculative_load_exec_(p->allow_speculative_load_exec)
    {
        // Pipeline collection config
        ldst_pipeline_.enableCollection(node);
        ldst_inst_queue_.enableCollection(node);

        load_queue_.enableCollection(node);
        store_queue_.enableCollection(node);


        // Startup handler for sending initial credits
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(LSU, sendInitialCredits_));


        // Port config
        in_lsu_insts_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getInstsFromDispatch_, InstPtr));

        in_biu_ack_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getAckFromBIU_, InstPtr));

        in_rob_retire_ack_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getAckFromROB_, InstPtr));

        in_reorder_flush_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, handleFlush_, FlushManager::FlushingCriteria));


        // Allow the pipeline to create events and schedule work
        ldst_pipeline_.performOwnUpdates();

        // There can be situations where NOTHING is going on in the
        // simulator but forward progression of the pipeline elements.
        // In this case, the internal event for the LS pipeline will
        // be the only event keeping simulation alive.  Sparta
        // supports identifying non-essential events (by calling
        // setContinuing to false on any event).
        ldst_pipeline_.setContinuing(true);

        ldst_pipeline_.registerHandlerAtStage(static_cast<uint32_t>(PipelineStage::MMU_LOOKUP),
                                                CREATE_SPARTA_HANDLER(LSU, handleMMULookupReq_));

        ldst_pipeline_.registerHandlerAtStage(static_cast<uint32_t>(PipelineStage::CACHE_LOOKUP),
                                                CREATE_SPARTA_HANDLER(LSU, handleCacheLookupReq_));

        ldst_pipeline_.registerHandlerAtStage(static_cast<uint32_t>(PipelineStage::COMPLETE),
                                                CREATE_SPARTA_HANDLER(LSU, completeInst_));


        // Event precedence setup
        uev_cache_drive_biu_port_ >> uev_mmu_drive_biu_port_;

        // NOTE:
        // To resolve the race condition when:
        // Both cache and MMU try to drive the single BIU port at the same cycle
        // Here we give cache the higher priority

        // DL1 cache config
        const uint32_t dl1_line_size = p->dl1_line_size;
        const uint32_t dl1_size_kb = p->dl1_size_kb;
        const uint32_t dl1_associativity = p->dl1_associativity;
        std::unique_ptr<sparta::cache::ReplacementIF> repl(new sparta::cache::TreePLRUReplacement
                                                         (dl1_associativity));
        dl1_cache_.reset(new SimpleDL1( getContainer(), dl1_size_kb, dl1_line_size, *repl ));
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

            if(inst_ptr->isStoreInst()){
                appendStoreQueue_(inst_info_ptr);
            }else if(allow_speculative_load_exec_){
                appendLoadQueue_(inst_info_ptr);
            }

            // Append to instruction issue queue
            appendIssueQueue_(inst_info_ptr);

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

    // Receive MSS access acknowledge from Bus Interface Unit
    void LSU::getAckFromBIU_(const InstPtr & inst_ptr)
    {
        if (inst_ptr == mmu_pending_inst_ptr_) {
            rehandleMMULookupReq_(inst_ptr);
        }
        else if (inst_ptr == cache_pending_inst_ptr_) {
            rehandleCacheLookupReq_(inst_ptr);
        }
        else {
            sparta_assert(false, "Unexpected BIU Ack event occurs!");
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
        // Instruction issue arbitration
        const LoadStoreInstInfoPtr & win_ptr = arbitrateInstIssue_();
        // NOTE:
        // win_ptr should always point to an instruction ready to be issued
        // Otherwise assertion error should already be fired in arbitrateInstIssue_()

        lsu_insts_issued_++;

        // Append load/store pipe
        ldst_pipeline_.append(win_ptr->getMemoryAccessInfoPtr());

        // Update instruction issue info
        win_ptr->setState(LoadStoreInstInfo::IssueState::ISSUED);
        win_ptr->setPriority(LoadStoreInstInfo::IssuePriority::LOWEST);

        // Schedule another instruction issue event if possible
        if (isReadyToIssueInsts_()) {
            uev_issue_inst_.schedule(sparta::Clock::Cycle(1));
        }

        ILOG("Issue/Re-issue Instruction: " << win_ptr->getInstPtr());
    }

    // Handle MMU access request
    void LSU::handleMMULookupReq_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::MMU_LOOKUP);

        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(stage_id)) {
            return;
        }


        const MemoryAccessInfoPtr & mem_access_info_ptr = ldst_pipeline_[stage_id];
        ILOG(mem_access_info_ptr);

        bool isAlreadyHIT = (mem_access_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::HIT);
        bool MMUBypass = isAlreadyHIT;

        if (MMUBypass) {
            ILOG("MMU Lookup is skipped (TLB is already hit)!");
            return;
        }

        // Access TLB, and check TLB hit or miss
        bool TLB_HIT = mmu_->Lookup(mem_access_info_ptr);

        if (TLB_HIT) {
            // Update memory access info
            mem_access_info_ptr->setMMUState(MemoryAccessInfo::MMUState::HIT);
            // Update physical address status
            mem_access_info_ptr->setPhyAddrStatus(true);
        }
        else {
            // Update memory access info
            mem_access_info_ptr->setMMUState(MemoryAccessInfo::MMUState::MISS);

            if (mmu_busy_ == false) {
                // MMU is busy, no more TLB MISS can be handled, RESET is required on finish
                mmu_busy_ = true;
                // Keep record of the current TLB MISS instruction
                mmu_pending_inst_ptr_ = mem_access_info_ptr->getInstPtr();

                // NOTE:
                // mmu_busy_ RESET could be done:
                // as early as port-driven event for this miss finish, and
                // as late as TLB reload event for this miss finish.

                // Schedule port-driven event in BIU
                uev_mmu_drive_biu_port_.schedule(sparta::Clock::Cycle(0));

                // NOTE:
                // The race between simultaneous MMU and cache requests is resolved by
                // specifying precedence between these two competing events
                ILOG("MMU is trying to drive BIU request port!");
            }
            else {
                ILOG("MMU miss cannot be served right now due to another outstanding one!");
            }

            // NEW: Invalidate pipeline stage
            ldst_pipeline_.invalidateStage(static_cast<uint32_t>(PipelineStage::MMU_LOOKUP));
        }
    }

    // Drive BIU request port from MMU
    void LSU::driveBIUPortFromMMU_()
    {
        bool succeed = false;

        // Check if DataOutPort is available
        if (!out_biu_req_.isDriven()) {
            sparta_assert(mmu_pending_inst_ptr_ != nullptr,
                "Attempt to drive BIU port when no outstanding TLB miss exists!");

            // Port is available, drive port immediately, and send request out
            out_biu_req_.send(mmu_pending_inst_ptr_);

            succeed = true;

            biu_reqs_++;
        }
        else {
            // Port is being driven by another source, wait for one cycle and check again
            uev_mmu_drive_biu_port_.schedule(sparta::Clock::Cycle(1));
        }

        if (succeed) {
            ILOG("MMU is driving the BIU request port!");
        }
        else {
            ILOG("MMU is waiting to drive the BIU request port!");
        }
    }

    // Handle cache access request
    void LSU::handleCacheLookupReq_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::CACHE_LOOKUP);

        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(stage_id)) {
            return;
        }


        const MemoryAccessInfoPtr & mem_access_info_ptr = ldst_pipeline_[stage_id];
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();

        ILOG(mem_access_info_ptr);

        const bool phyAddrIsReady =
            mem_access_info_ptr->getPhyAddrStatus();
        const bool isAlreadyHIT =
            (mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT);
        const bool isUnretiredStore =
            inst_ptr->isStoreInst() && (inst_ptr->getStatus() != Inst::Status::RETIRED);
        const bool cacheBypass = isAlreadyHIT || !phyAddrIsReady || isUnretiredStore;

        if (cacheBypass) {
            if (isAlreadyHIT) {
                ILOG("Cache Lookup is skipped (Cache already hit)!");
            }
            else if (!phyAddrIsReady) {
                ILOG("Cache Lookup is skipped (Physical address not ready)!");
            }
            else if (isUnretiredStore) {
                ILOG("Cache Lookup is skipped (Un-retired store instruction)!");
            }
            else {
                sparta_assert(false, "Cache access is bypassed without a valid reason!");
            }
            return;
        }

        // Access cache, and check cache hit or miss
        const bool CACHE_HIT = cacheLookup_(mem_access_info_ptr);

        if (CACHE_HIT) {
            // Update memory access info
            mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
        }
        else {
            // Update memory access info
            mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);

            if (cache_busy_ == false) {
                // Cache is now busy, no more CACHE MISS can be handled, RESET required on finish
                cache_busy_ = true;
                // Keep record of the current CACHE MISS instruction
                cache_pending_inst_ptr_ = mem_access_info_ptr->getInstPtr();

                // NOTE:
                // cache_busy_ RESET could be done:
                // as early as port-driven event for this miss finish, and
                // as late as cache reload event for this miss finish.

                // Schedule port-driven event in BIU
                uev_cache_drive_biu_port_.schedule(sparta::Clock::Cycle(0));

                // NOTE:
                // The race between simultaneous MMU and cache requests is resolved by
                // specifying precedence between these two competing events
                ILOG("Cache is trying to drive BIU request port!");
            }
            else {
                ILOG("Cache miss cannot be served right now due to another outstanding one!");
            }

            // NEW: Invalidate pipeline stage
            ldst_pipeline_.invalidateStage(static_cast<uint32_t>(PipelineStage::CACHE_LOOKUP));
        }
    }

    // Drive BIU request port from cache
    void LSU::driveBIUPortFromCache_()
    {
        bool succeed = false;

        // Check if DataOutPort is available
        if (!out_biu_req_.isDriven()) {
            sparta_assert(cache_pending_inst_ptr_ != nullptr,
                "Attempt to drive BIU port when no outstanding cache miss exists!");

            // Port is available, drive the port immediately, and send request out
            out_biu_req_.send(cache_pending_inst_ptr_);

            succeed = true;

            biu_reqs_++;
        }
        else {
            // Port is being driven by another source, wait for one cycle and check again
            uev_cache_drive_biu_port_.schedule(sparta::Clock::Cycle(1));
        }

        if (succeed) {
            ILOG("Cache is driving the BIU request port!");
        }
        else {
            ILOG("Cache is waiting to drive the BIU request port!");
        }
    }

    // Retire load/store instruction
    void LSU::completeInst_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::COMPLETE);

        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(stage_id)) {
            return;
        }


        const MemoryAccessInfoPtr & mem_access_info_ptr = ldst_pipeline_[stage_id];
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        bool isStoreInst = inst_ptr->isStoreInst();
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
        if (!isStoreInst) {
            sparta_assert(mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT,
                        "Load instruction cannot complete when cache is still a miss!");

            // Update instruction status
            inst_ptr->setStatus(Inst::Status::COMPLETED);

            lsu_insts_completed_++;

            // Remove completed instruction from issue queue
            popIssueQueue_(inst_ptr);

            // Remove completed instruction from load queue
            popLoadQueue_(inst_ptr);

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

            lsu_insts_completed_++;

            // Remove store instruction from issue queue
            popIssueQueue_(inst_ptr);

            // Remove completed instruction from store queue
            popStoreQueue_(inst_ptr);

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
        if (mmu_busy_ && flush(mmu_pending_inst_ptr_->getUniqueID())) {
            mmu_pending_inst_flushed = true;
        }

        // Mark flushed flag for unfinished speculative cache access
        if (cache_busy_ && flush(cache_pending_inst_ptr_->getUniqueID())) {
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
        ldst_inst_queue_.push_back(inst_info_ptr);

        ILOG("Append new load/store instruction to issue queue!");
    }

    // Pop completed load/store instruction out of issue queue
    void LSU::popIssueQueue_(const InstPtr & inst_ptr)
    {
        // Look for the instruction to be completed, and remove it from issue queue
        for (auto iter = ldst_inst_queue_.begin(); iter != ldst_inst_queue_.end(); iter++) {
            if ((*iter)->getInstPtr() == inst_ptr) {
                ldst_inst_queue_.erase(iter);

                return;
            }
        }

        sparta_assert(false, "Attempt to complete instruction no longer exiting in issue queue!");
    }

    // Pop completed load instruction out of load queue
    void LSU::popLoadQueue_(const InstPtr & inst_ptr)
    {
        // Look for the instruction to be completed, and remove it from load queue
        for (auto iter = load_queue_.begin(); iter != load_queue_.end(); iter++) {
            if ((*iter)->getInstPtr() == inst_ptr) {
                load_queue_.erase(iter);

                return;
            }
        }

        sparta_assert(false, "Attempt to complete instruction no longer exiting in issue queue!");
    }

    // Pop completed store instruction out of store queue
    void LSU::popStoreQueue_(const InstPtr & inst_ptr)
    {
        // Look for the instruction to be completed, and remove it from store queue
        for (auto iter = store_queue_.begin(); iter != store_queue_.end(); iter++) {
            if ((*iter)->getInstPtr() == inst_ptr) {
                store_queue_.erase(iter);

                return;
            }
        }

        sparta_assert(false, "Attempt to complete instruction no longer exiting in issue queue!");
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

            // Pick winner
            if (!(*win_ptr_iter)->isReady() || (*iter)->winArb(*win_ptr_iter)) {
                win_ptr_iter = iter;
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

    // Append new load instruction into issue queue
    void LSU::appendLoadQueue_(const LoadStoreInstInfoPtr & inst_info_ptr)
    {
        sparta_assert(load_queue_.size() <= ld_queue_size_,
                      "Appending load queue causes overflows!");

        // Always append newly dispatched instructions to the back of load queue
        load_queue_.push_back(inst_info_ptr);

        ILOG("Append new load instruction to load queue!");
    }

    // Append new store instruction into issue queue
    void LSU::appendStoreQueue_(const LoadStoreInstInfoPtr & inst_info_ptr)
    {
        sparta_assert(store_queue_.size() <= st_queue_size_,
                      "Appending store queue causes overflows!");

        // Always append newly dispatched instructions to the back of store queue
        store_queue_.push_back(inst_info_ptr);

        ILOG("Append new store instruction to store queue!");
    }

    // Re-handle outstanding MMU access request
    void LSU::rehandleMMULookupReq_(const InstPtr & inst_ptr)
    {
        // MMU is no longer busy any more
        mmu_busy_ = false;
        mmu_pending_inst_ptr_.reset();

        // NOTE:
        // MMU may not have to wait until MSS Ack comes back
        // MMU could be ready to service new TLB MISS once previous request has been sent
        // However, that means MMU has to keep record of a list of pending instructions

        // Check if this MMU miss Ack is for an already flushed instruction
        if (mmu_pending_inst_flushed) {
            mmu_pending_inst_flushed = false;
            ILOG("BIU Ack for a flushed MMU miss is received!");

            // Schedule an instruction (re-)issue event
            // Note: some younger load/store instruction(s) might have been blocked by
            // this outstanding miss
            updateIssuePriorityAfterTLBReload_(inst_ptr, true);
            if (isReadyToIssueInsts_()) {
                uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
            }
            return;
        }

        ILOG("BIU Ack for an outstanding MMU miss is received!");

        // Reload TLB entry
        mmu_->reloadTLB_(inst_ptr->getTargetVAddr());

        // Update issue priority & Schedule an instruction (re-)issue event
        updateIssuePriorityAfterTLBReload_(inst_ptr);
        uev_issue_inst_.schedule(sparta::Clock::Cycle(0));

        ILOG("MMU rehandling event is scheduled!");
    }

    // Access Cache
    bool LSU::cacheLookup_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        uint64_t phyAddr = inst_ptr->getRAdr();

        bool cache_hit = false;

        if (dl1_always_hit_) {
            cache_hit = true;
        }
        else {
            auto cache_line = dl1_cache_->peekLine(phyAddr);
            cache_hit = (cache_line != nullptr) && cache_line->isValid();

            // Update MRU replacement state if Cache HIT
            if (cache_hit) {
                dl1_cache_->touchMRU(*cache_line);
            }
        }

        if (dl1_always_hit_) {
            ILOG("DL1 Cache HIT all the time: phyAddr=0x" << std::hex << phyAddr);
            dl1_cache_hits_++;
        }
        else if (cache_hit) {
            ILOG("DL1 Cache HIT: phyAddr=0x" << std::hex << phyAddr);
            dl1_cache_hits_++;
        }
        else {
            ILOG("DL1 Cache MISS: phyAddr=0x" << std::hex << phyAddr);
            dl1_cache_misses_++;
        }

        return cache_hit;
    }

    // Re-handle outstanding cache access request
    void LSU::rehandleCacheLookupReq_(const InstPtr & inst_ptr)
    {
        // Cache is no longer busy any more
        cache_busy_ = false;
        cache_pending_inst_ptr_.reset();

        // NOTE:
        // Depending on cache is blocking or not,
        // It may not have to wait until MMS Ack returns.
        // However, that means cache has to keep record of a list of pending instructions

        // Check if this cache miss Ack is for an already flushed instruction
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

        ILOG("BIU Ack for an outstanding cache miss is received!");

        // Reload cache line
        reloadCache_(inst_ptr->getRAdr());

        // Update issue priority & Schedule an instruction (re-)issue event
        updateIssuePriorityAfterCacheReload_(inst_ptr);
        uev_issue_inst_.schedule(sparta::Clock::Cycle(0));

        ILOG("Cache rehandling event is scheduled!");
    }

    // Reload cache line
    void LSU::reloadCache_(uint64_t phyAddr)
    {
        auto dl1_cache_line = &dl1_cache_->getLineForReplacementWithInvalidCheck(phyAddr);
        dl1_cache_->allocateWithMRUUpdate(*dl1_cache_line, phyAddr);

        ILOG("Cache reload complete!");
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