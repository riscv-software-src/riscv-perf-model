#include "sparta/utils/SpartaAssert.hpp"
#include "CoreUtils.hpp"
#include "LSU.hpp"
#include <string>

#include "OlympiaAllocators.hpp"

namespace olympia
{
    const char LSU::name[] = "lsu";

    ////////////////////////////////////////////////////////////////////////////////
    // Constructor
    ////////////////////////////////////////////////////////////////////////////////

    LSU::LSU(sparta::TreeNode *node, const LSUParameterSet *p) :
        sparta::Unit(node),
        ldst_inst_queue_("lsu_inst_queue", p->ldst_inst_queue_size, getClock()),
        ldst_inst_queue_size_(p->ldst_inst_queue_size),
        replay_buffer_("replay_buffer", p->replay_buffer_size, getClock()),
        replay_buffer_size_(p->replay_buffer_size),
        replay_issue_delay_(p->replay_issue_delay),
        load_store_info_allocator_(sparta::notNull(OlympiaAllocators::getOlympiaAllocators(node))->
                                   load_store_info_allocator),
        memory_access_allocator_(sparta::notNull(OlympiaAllocators::getOlympiaAllocators(node))->
                                 memory_access_allocator),
        address_calculation_stage_(0),
        mmu_lookup_stage_(address_calculation_stage_ + p->mmu_lookup_stage_length),
        cache_lookup_stage_(mmu_lookup_stage_ + p->cache_lookup_stage_length),
        cache_read_stage_(cache_lookup_stage_ + p->cache_read_stage_length),
        complete_stage_(cache_read_stage_ + 1), // Complete stage is after the cache read stage
        ldst_pipeline_("LoadStorePipeline",(complete_stage_ + 1), getClock()),
        allow_speculative_load_exec_(p->allow_speculative_load_exec)
    {
        sparta_assert(p->mmu_lookup_stage_length > 0, "MMU lookup stage should atleast be one cycle");
        sparta_assert(p->cache_read_stage_length > 0, "Cache read stage should atleast be one cycle");
        sparta_assert(p->cache_lookup_stage_length > 0, "Cache lookup stage should atleast be one cycle");
        // Pipeline collection config
        ldst_pipeline_.enableCollection(node);
        ldst_inst_queue_.enableCollection(node);

        replay_buffer_.enableCollection(node);

        // Startup handler for sending initial credits
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(LSU, sendInitialCredits_));


        // Port config
        in_lsu_insts_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getInstsFromDispatch_, InstPtr));

        in_rob_retire_ack_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getAckFromROB_, InstPtr));

        in_reorder_flush_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, handleFlush_,
                                             FlushManager::FlushingCriteria));

        in_mmu_lookup_req_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(LSU, handleMMUReadyReq_,
                                            MemoryAccessInfoPtr));

        in_mmu_lookup_ack_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getAckFromMMU_,
                                             MemoryAccessInfoPtr));

        in_cache_lookup_req_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(LSU, handleCacheReadyReq_,
                                            MemoryAccessInfoPtr));

        in_cache_lookup_ack_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getAckFromCache_,
                                             MemoryAccessInfoPtr));

        // Allow the pipeline to create events and schedule work
        ldst_pipeline_.performOwnUpdates();

        // There can be situations where NOTHING is going on in the
        // simulator but forward progression of the pipeline elements.
        // In this case, the internal event for the LS pipeline will
        // be the only event keeping simulation alive.  Sparta
        // supports identifying non-essential events (by calling
        // setContinuing to false on any event).
        ldst_pipeline_.setContinuing(true);

        ldst_pipeline_.registerHandlerAtStage
            (address_calculation_stage_,
             CREATE_SPARTA_HANDLER(LSU, handleAddressCalculation_));

        ldst_pipeline_.registerHandlerAtStage
            (mmu_lookup_stage_,
             CREATE_SPARTA_HANDLER(LSU, handleMMULookupReq_));

        ldst_pipeline_.registerHandlerAtStage
            (cache_lookup_stage_,
             CREATE_SPARTA_HANDLER(LSU, handleCacheLookupReq_));

        ldst_pipeline_.registerHandlerAtStage
            (cache_read_stage_,
             CREATE_SPARTA_HANDLER(LSU, handleCacheRead_));

        ldst_pipeline_.registerHandlerAtStage
            (complete_stage_,
             CREATE_SPARTA_HANDLER(LSU, completeInst_));

        // NOTE:
        // To resolve the race condition when:
        // Both cache and MMU try to drive the single BIU port at the same cycle
        // Here we give cache the higher priority
        ILOG("LSU construct: #" << node->getGroupIdx());

    }


    LSU::~LSU()
    {
        DLOG(
            getContainer()->getLocation()
            << ": " << load_store_info_allocator_.getNumAllocated() << " LoadStoreInstInfo objects allocated/created"
        );
        DLOG(
            getContainer()->getLocation()
            << ": " << memory_access_allocator_.getNumAllocated() << " MemoryAccessInfo objects allocated/created");
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
        if(!instInQueue_(ldst_inst_queue_, inst_ptr)){
            ILOG("New instruction added to the ldst queue " << inst_ptr);
            allocateInstToQueues_(inst_ptr);
            lsu_insts_dispatched_++;
        }

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
            ILOG("Instruction NOT ready: " << inst_ptr << " Address Bits needed:" << sparta::printBitSet(src_bits));
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
                    ILOG("Instruction NOT ready: " << inst_ptr << " Data Bits needed:" << sparta::printBitSet(data_bits));
                }
            }else if (false == allow_speculative_load_exec_){ // Its a load
                // Load instruction is ready is when both address and older stores addresses are known
                all_ready = allOlderStoresIssued_(inst_ptr);
                if(all_ready)
                {
                    ILOG("Load instruction " << inst_ptr << " is ready to be scheduled");
                }else{
                    ILOG("Load instruction " << inst_ptr << " not dispatched");
                }
            }
        }

        if (all_ready) {
            // Update issue priority & Schedule an instruction issue event
            updateIssuePriorityAfterNewDispatch_(inst_ptr);

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

            if(isReadyToIssueInsts_())
            {
                uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
            }
        }
    }

    // Receive update from ROB whenever store instructions retire
    void LSU::getAckFromROB_(const InstPtr & inst_ptr)
    {
        sparta_assert(inst_ptr->getStatus() == Inst::Status::RETIRED,
                      "Get ROB Ack, but the store inst hasn't retired yet!");

        ++stores_retired_;

        updateIssuePriorityAfterStoreInstRetire_(inst_ptr);
        if(isReadyToIssueInsts_())
        {
            ILOG("ROB Ack issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }

        ILOG("ROB Ack: Retired store instruction: " << inst_ptr);
    }

    // Issue/Re-issue ready instructions in the issue queue
    void LSU::issueInst_()
    {
        if(allow_speculative_load_exec_ && replay_buffer_.size() > replay_buffer_size_){
            ILOG("Replay buffer full");
            return;
        }
        // Instruction issue arbitration
        const LoadStoreInstInfoPtr & win_ptr = arbitrateInstIssue_();
        // NOTE:
        // win_ptr should always point to an instruction ready to be issued
        // Otherwise assertion error should already be fired in arbitrateInstIssue_()
        ILOG("Arbitrated inst " << win_ptr << " " << win_ptr->getInstPtr());

        lsu_insts_issued_++;

        if(allow_speculative_load_exec_)
        {
            ILOG("Appending to replay queue " << win_ptr);
            appendToQueue_(replay_buffer_, win_ptr);
        }

        // Append load/store pipe
        ldst_pipeline_.append(win_ptr);

        // Update instruction issue info
        win_ptr->setState(LoadStoreInstInfo::IssueState::ISSUED);
        win_ptr->setPriority(LoadStoreInstInfo::IssuePriority::LOWEST);

        // Schedule another instruction issue event if possible
        if (isReadyToIssueInsts_()) {
            ILOG("IssueInst_ issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(1));
        }

        ILOG("Issue/Re-issue Instruction: " << win_ptr->getInstPtr());
    }

    void LSU::handleAddressCalculation_()
    {
        auto stage_id = address_calculation_stage_;

        if (!ldst_pipeline_.isValid(stage_id)) {
            return;
        }

        auto & ldst_info_ptr = ldst_pipeline_[stage_id];
        auto & inst_ptr = ldst_info_ptr->getInstPtr();
        // Assume Calculate Address

        ILOG("Address Generation " << inst_ptr << ldst_info_ptr);

    }

    ////////////////////////////////////////////////////////////////////////////////
    // MMU subroutines
    ////////////////////////////////////////////////////////////////////////////////
    // Handle MMU access request
    void LSU::handleMMULookupReq_()
    {
        const auto stage_id = mmu_lookup_stage_;

        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(stage_id)) {
            return;
        }

        const LoadStoreInstInfoPtr &load_store_info_ptr = ldst_pipeline_[stage_id];
        const MemoryAccessInfoPtr &mem_access_info_ptr = load_store_info_ptr->getMemoryAccessInfoPtr();

        bool mmu_bypass = (mem_access_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::HIT);

        if (mmu_bypass) {
            ILOG("MMU Lookup is skipped (TLB is already hit)! " << load_store_info_ptr);
            return;
        }

        // Ready dependent younger loads

        if (false == allow_speculative_load_exec_)
        {
            if (mem_access_info_ptr->getInstPtr()->isStoreInst())
            {
                readyDependentLoads_(load_store_info_ptr);
            }
        }

        out_mmu_lookup_req_.send(mem_access_info_ptr);
        ILOG(mem_access_info_ptr << load_store_info_ptr);
    }

    void LSU::getAckFromMMU_(const MemoryAccessInfoPtr &updated_memory_access_info_ptr)
    {
        const auto stage_id = mmu_lookup_stage_;

        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(stage_id)) {
            ILOG("MMU stage not valid");
            return;
        }
        ILOG("MMU Ack: "
             << std::boolalpha << updated_memory_access_info_ptr->getPhyAddrStatus()
             << " " << updated_memory_access_info_ptr);
        const bool mmu_hit_ = updated_memory_access_info_ptr->getPhyAddrStatus();

        if(updated_memory_access_info_ptr->getInstPtr()->isStoreInst() && mmu_hit_ && allow_speculative_load_exec_){
            ILOG("Aborting speculative loads " << updated_memory_access_info_ptr);
            abortYoungerLoads_(updated_memory_access_info_ptr);
        }
    }

    void LSU::handleMMUReadyReq_(const MemoryAccessInfoPtr &memory_access_info_ptr)
    {
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

        updateIssuePriorityAfterTLBReload_(inst_ptr);
        if(isReadyToIssueInsts_())
        {
            ILOG("MMU ready issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }

        ILOG("MMU rehandling event is scheduled! " << memory_access_info_ptr);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Cache Subroutine
    ////////////////////////////////////////////////////////////////////////////////
    // Handle cache access request
    void LSU::handleCacheLookupReq_()
    {
        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(cache_lookup_stage_)) {
            return;
        }

        const LoadStoreInstInfoPtr &load_store_info_ptr = ldst_pipeline_[cache_lookup_stage_];
        const MemoryAccessInfoPtr &mem_access_info_ptr = load_store_info_ptr->getMemoryAccessInfoPtr();
        const bool phy_addr_is_ready = mem_access_info_ptr->getPhyAddrStatus();

        // If we did not have an MMU hit from previous stage, invalidate and bail
        if(false == phy_addr_is_ready) {
            ILOG("Cache Lookup is skipped (Physical address not ready)!" << load_store_info_ptr);
            if(allow_speculative_load_exec_)
            {
                updateInstReplayReady_(mem_access_info_ptr->getInstPtr());
            }
            ldst_pipeline_.invalidateStage(cache_lookup_stage_);
            return;
        }

        const InstPtr &inst_ptr = mem_access_info_ptr->getInstPtr();
        ILOG(load_store_info_ptr << " " << mem_access_info_ptr);

        // If have passed translation and the instruction is a store,
        // then it's good to be retired (i.e. mark it completed).
        // Stores typically do not cause a flush after a successful
        // translation.  We now wait for the Retire block to "retire"
        // it, meaning it's good to go to the cache
        if(inst_ptr->isStoreInst() && (inst_ptr->getStatus() != Inst::Status::RETIRED)) {
            ILOG("Store marked as completed " << inst_ptr);
            inst_ptr->setStatus(Inst::Status::COMPLETED);
            load_store_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
            ldst_pipeline_.invalidateStage(cache_lookup_stage_);
            if(allow_speculative_load_exec_)
            {
                updateInstReplayReady_(inst_ptr);
            }
            return;
        }

        if(!inst_ptr->isStoreInst() && olderStoresExists_(inst_ptr) && allow_speculative_load_exec_){
            ILOG("Dropping speculative load " << inst_ptr);
            load_store_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
            ldst_pipeline_.invalidateStage(cache_lookup_stage_);
            if(allow_speculative_load_exec_)
            {
                updateInstReplayReady_(inst_ptr);
            }
            return;
        }

        const bool is_already_hit =
                (mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT);
        const bool is_unretired_store =
                inst_ptr->isStoreInst() && (inst_ptr->getStatus() != Inst::Status::RETIRED);
        const bool cache_bypass = is_already_hit || !phy_addr_is_ready || is_unretired_store;

        if (cache_bypass) {
            if (is_already_hit) {
                ILOG("Cache Lookup is skipped (Cache already hit)");
            }
            else if (is_unretired_store) {
                ILOG("Cache Lookup is skipped (store instruction not oldest)");
            }
            else {
                sparta_assert(false, "Cache access is bypassed without a valid reason!");
            }
            return;
        }

        out_cache_lookup_req_.send(mem_access_info_ptr);
    }

    void LSU::getAckFromCache_(const MemoryAccessInfoPtr &updated_memory_access_info_ptr){
    }

    void LSU::handleCacheReadyReq_(const MemoryAccessInfoPtr &memory_access_info_ptr)
    {
        auto inst_ptr = memory_access_info_ptr->getInstPtr();
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

        if(inst_ptr->getUniqueID() == 33)
        ILOG("Cache ready for " << memory_access_info_ptr);
        updateIssuePriorityAfterCacheReload_(inst_ptr);

        if (isReadyToIssueInsts_())
        {
            ILOG("Cache ready issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    void LSU::handleCacheRead_()
    {
        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(cache_read_stage_)) {
            return;
        }

        const LoadStoreInstInfoPtr &load_store_info_ptr = ldst_pipeline_[cache_read_stage_];
        const MemoryAccessInfoPtr &mem_access_info_ptr = load_store_info_ptr->getMemoryAccessInfoPtr();
        ILOG(mem_access_info_ptr);

        if(false == mem_access_info_ptr->isCacheHit()) {
            ILOG("Cannot complete inst, cache miss: " << mem_access_info_ptr);
            if(allow_speculative_load_exec_)
            {
                updateInstReplayReady_(mem_access_info_ptr->getInstPtr());
            }
            ldst_pipeline_.invalidateStage(cache_read_stage_);
            return;
        }

        if(mem_access_info_ptr->isDataReady()){
            ILOG("Instruction had previously had its data ready");
            return;
        }

        ILOG("Data ready set for " << mem_access_info_ptr);
        mem_access_info_ptr->setDataReady(true);

        if (isReadyToIssueInsts_())
        {
            ILOG("Cache read issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Retire load/store instruction
    void LSU::completeInst_()
    {
        const auto stage_id = complete_stage_;

        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(stage_id)) {
            return;
        }

        const LoadStoreInstInfoPtr &load_store_info_ptr = ldst_pipeline_[stage_id];
        const MemoryAccessInfoPtr &mem_access_info_ptr = load_store_info_ptr->getMemoryAccessInfoPtr();

        if(false == mem_access_info_ptr->isDataReady()) {
            ILOG("Cannot complete inst, cache data is missing: " << mem_access_info_ptr);
            return;
        }

        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        const bool is_store_inst = inst_ptr->isStoreInst();
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
                          "Load instruction cannot complete when cache is still a miss! " << mem_access_info_ptr);

            // Update instruction status
            inst_ptr->setStatus(Inst::Status::COMPLETED);

            // Remove completed instruction from queues
            ILOG("Removed issue queue "  << inst_ptr);
            popIssueQueue_(inst_ptr);

            if(allow_speculative_load_exec_)
            {
                ILOG("Removed replay " << inst_ptr);
                removeFromQueue_(replay_buffer_, load_store_info_ptr);
            }

            lsu_insts_completed_++;
            out_lsu_credits_.send(1, 0);

            if(isReadyToIssueInsts_() && !ldst_pipeline_.isStalledOrStalling())
            {
                ILOG("Complete issue");
                uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
            }

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

            if(isReadyToIssueInsts_() && !ldst_pipeline_.isStalledOrStalling())
            {
                ILOG("Store complete issue");
                uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
            }
        }
        // Finish store operation
        else {
            sparta_assert(mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT,
                        "Store inst cannot finish when cache is still a miss! " << inst_ptr);

            sparta_assert(mem_access_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::HIT,
                          "Store inst cannot finish when cache is still a miss! " << inst_ptr);

            ILOG("Removed issue queue "  << inst_ptr);
            popIssueQueue_(inst_ptr);

            if(allow_speculative_load_exec_)
            {
                ILOG("Removed replay " << load_store_info_ptr);
                removeFromQueue_(replay_buffer_, load_store_info_ptr);
            }

            lsu_insts_completed_++;
            out_lsu_credits_.send(1, 0);

            if(isReadyToIssueInsts_() && !ldst_pipeline_.isStalledOrStalling())
            {
                ILOG("Complete store issue");
                uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
            }

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
    void LSU::replayReady_(const InstPtr & replay_inst_ptr)
    {
        ILOG("Replay inst ready " << replay_inst_ptr);
        // We check in the ldst_queue as the instruction may not be in the replay queue
        for (auto & load_store_info_ptr : ldst_inst_queue_)
        {
            auto & inst_ptr = load_store_info_ptr->getInstPtr();
            if (inst_ptr == replay_inst_ptr)
            {
                if(load_store_info_ptr->getState() == LoadStoreInstInfo::IssueState::NOT_READY)
                {
                    load_store_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                }
                auto issue_priority = load_store_info_ptr->getMemoryAccessInfoPtr()->getPhyAddrStatus()
                                          ? LoadStoreInstInfo::IssuePriority::CACHE_PENDING
                                          : LoadStoreInstInfo::IssuePriority::MMU_PENDING;
                load_store_info_ptr->setPriority(issue_priority);
            }
        }
        if (isReadyToIssueInsts_())
        {
            ILOG("replay ready issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    void LSU::updateInstReplayReady_(const InstPtr &inst_ptr){
        for(auto iter = replay_buffer_.begin(); iter != replay_buffer_.end(); iter++)
        {
            auto &load_store_info_ptr = *iter;
            if (load_store_info_ptr->getInstPtr() == inst_ptr)
            {
                ILOG("Scheduled replay " << load_store_info_ptr << " after " << replay_issue_delay_ << " cycles");
                load_store_info_ptr->setState(LoadStoreInstInfo::IssueState::NOT_READY);
                uev_replay_ready_.preparePayload(inst_ptr)->schedule(sparta::Clock::Cycle(replay_issue_delay_));
                replay_buffer_.erase(iter);

                replay_insts_++;
                return;
            }
        }

        sparta_assert(false, "Cannot replay a non existing instruction " << inst_ptr);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Regular Function/Subroutine Call
    ////////////////////////////////////////////////////////////////////////////////

    bool LSU::instPresentInQueues(const InstPtr &inst_ptr)
    {
        return instInQueue_(ldst_inst_queue_, inst_ptr);
    }

    bool LSU::instInQueue_(LoadStoreIssueQueue &queue,const InstPtr &inst_ptr)
    {
        return std::any_of(queue.begin(), queue.end(),
                           [inst_ptr](auto & ldst_info_ptr)
                           { return inst_ptr == ldst_info_ptr->getInstPtr(); });
    }

    LSU::LoadStoreInstInfoPtr LSU::createLoadStoreInst_(const InstPtr &inst_ptr){
        // Create load/store memory access info
        MemoryAccessInfoPtr mem_info_ptr = sparta::allocate_sparta_shared_pointer<MemoryAccessInfo>(memory_access_allocator_,
                                                                                                    inst_ptr);
        // Create load/store instruction issue info
        LoadStoreInstInfoPtr inst_info_ptr = sparta::allocate_sparta_shared_pointer<LoadStoreInstInfo>(load_store_info_allocator_,
                                                                                                       mem_info_ptr);
        return inst_info_ptr;
    }

    void LSU::allocateInstToQueues_(const InstPtr &inst_ptr)
    {
        auto inst_info_ptr = createLoadStoreInst_(inst_ptr);
        appendIssueQueue_(inst_info_ptr);
    }

    bool LSU::allOlderStoresIssued_(const InstPtr &inst_ptr){
        return olderStoresIssued_(ldst_inst_queue_, inst_ptr);
    }

    bool LSU::olderStoresIssued_(LoadStoreIssueQueue &queue, const InstPtr &inst_ptr)
    {
        for(const auto &ldst_info_ptr: queue){
            const auto &ldst_inst_ptr = ldst_info_ptr->getInstPtr();
            const auto &mem_info_ptr = ldst_info_ptr->getMemoryAccessInfoPtr();
            if(ldst_inst_ptr->isStoreInst() &&
                ldst_inst_ptr->getUniqueID() < inst_ptr->getUniqueID() &&
                !mem_info_ptr->getPhyAddrStatus() &&
                ldst_info_ptr->getInstPtr() != inst_ptr){
                return false;
            }
        }
        return true;
    }

    // Only called if allow_spec_load_exec is true
    void LSU::readyDependentLoads_(const LoadStoreInstInfoPtr &store_inst_ptr){
        bool found = false;
        for(auto &ldst_inst_ptr: ldst_inst_queue_){
            auto &inst_ptr = ldst_inst_ptr->getInstPtr();
            if(inst_ptr->isStoreInst()){
                continue;
            }

            // Only ready loads which have register operands ready
            if(inst_ptr->getStatus() == Inst::Status::DISPATCHED &&
                scoreboard_views_[core_types::RF_INTEGER]->isSet(inst_ptr->getSrcRegisterBitMask(core_types::RF_INTEGER))){
                ILOG("Updating inst to schedule " << inst_ptr << " " << ldst_inst_ptr);
                updateIssuePriorityAfterNewDispatch_(inst_ptr);
                inst_ptr->setStatus(Inst::Status::SCHEDULED);
                found = true;
            }
        }

        if (found && isReadyToIssueInsts_() && !ldst_pipeline_.isStalledOrStalling())
        {
            ILOG("Ready dep inst ");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    void LSU::abortYoungerLoads_(const olympia::MemoryAccessInfoPtr & memory_access_info_ptr)
    {
        auto & inst_ptr = memory_access_info_ptr->getInstPtr();
        deallocateYoungerLoadFromQueue_(replay_buffer_, inst_ptr);
    }

    void LSU::deallocateYoungerLoadFromQueue_(LoadStoreIssueQueue & queue, const InstPtr & inst_ptr)
    {
        uint64_t min_inst_age = UINT64_MAX;
        // Find oldest instruction age with the same Virtual address
        for (auto iter = queue.begin(); iter != queue.end(); iter++)
        {
            auto & queue_inst = (*iter)->getInstPtr();
            if(queue_inst->isStoreInst()){
                continue;
            }
            if(queue_inst == inst_ptr){
                continue;
            }
            if (queue_inst->getTargetVAddr() == inst_ptr->getTargetVAddr() &&
                queue_inst->getUniqueID() > inst_ptr->getUniqueID())
            {
                if (queue_inst->getUniqueID() < min_inst_age)
                {
                    min_inst_age = queue_inst->getUniqueID();
                }
            }
        }
        if(min_inst_age == UINT64_MAX){
            ILOG("No younger instruction to deallocate");
            return;
        }
        ILOG("Age of the oldest instruction " << min_inst_age << " for " << inst_ptr << inst_ptr->getTargetVAddr());
        // Remove instructions younger than the oldest load that was removed
        auto iter = queue.begin();
        while(iter != queue.end()){
            if((*iter)->getInstPtr() == inst_ptr){
                ++iter;
                continue;
            }
            if ((*iter)->getInstUniqueID() >= min_inst_age){
                (*iter)->setState(LoadStoreInstInfo::IssueState::READY);
                ILOG("Aborted younger load " << *iter << (*iter)->getInstPtr()->getTargetVAddr());
                invalidatePipeline_((*iter)->getInstPtr());
                (*iter)->getMemoryAccessInfoPtr()->reset();
                queue.erase(iter++);
            }else{
                ++iter;
            }
        }
    }

    void LSU::invalidatePipeline_(const InstPtr & inst_ptr){
        ILOG("InvalidatePipeline called");

        for(int stage = 0; stage <= complete_stage_; stage++){
            if(ldst_pipeline_.isValid(stage)){
                auto &pipeline_inst = ldst_pipeline_[stage]->getInstPtr();
                if(pipeline_inst == inst_ptr){
                    ldst_pipeline_.invalidateStage(stage);
                }
            }
        }
    }
    // Append new load/store instruction into issue queue
    void LSU::appendIssueQueue_(const LoadStoreInstInfoPtr & inst_info_ptr)
    {
        sparta_assert(ldst_inst_queue_.size() < ldst_inst_queue_size_,
                        "Appending issue queue causes overflows!");

        // Always append newly dispatched instructions to the back of issue queue
        ldst_inst_queue_.push_back(inst_info_ptr);

        ILOG("Append new load/store instruction to issue queue!");
    }

    void inline LSU::removeFromQueue_(LoadStoreIssueQueue &queue, const LoadStoreInstInfoPtr & inst_to_remove){
        ILOG("Removing " << inst_to_remove << " from queue");
        for (auto iter = queue.begin(); iter != queue.end(); iter++) {
            if ((*iter)->getInstPtr() == inst_to_remove->getInstPtr()) {
                queue.erase(iter);

                return;
            }
        }

        sparta_assert(false, "Attempt to remove instruction no longer existing in the queue!");
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

        sparta_assert(false, "Attempt to complete instruction no longer exiting in issue queue! " << inst_ptr);
    }

    void inline LSU::appendToQueue_(LoadStoreIssueQueue &queue, const LoadStoreInstInfoPtr &inst_info_ptr){
        sparta_assert(queue.size() < ldst_inst_queue_size_,
                      "Appending load queue causes overflows!");

        bool exists = std::find_if(
            queue.begin(), queue.end(),
            [inst_info_ptr](auto &queue_inst_ptr) { return queue_inst_ptr->getInstPtr() == inst_info_ptr->getInstPtr(); }
        ) == std::end(queue);

        sparta_assert(exists,  "Cannot push duplicate instructions into the queue " << inst_info_ptr);

        // Always append newly dispatched instructions to the back of issue queue
        queue.push_back(inst_info_ptr);

        ILOG("Append new instruction to queue!");
    }

    // Arbitrate instruction issue from ldst_inst_queue
    const LSU::LoadStoreInstInfoPtr & LSU::arbitrateInstIssue_()
    {
        sparta_assert(ldst_inst_queue_.size() > 0, "Arbitration fails: issue is empty!");

        auto win_ptr_iter = replay_buffer_.begin();
        if(allow_speculative_load_exec_)
        {
            for (auto iter = replay_buffer_.begin(); iter != replay_buffer_.end(); iter++)
            {
                if (!(*iter)->isReady())
                {
                    continue;
                }
                if (!(*win_ptr_iter)->isReady() || ((*iter)->winArb(*win_ptr_iter)))
                {
                    win_ptr_iter = iter;
                }
            }
            if (win_ptr_iter.isValid() && (*win_ptr_iter)->isReady() && allow_speculative_load_exec_)
            {
                return *win_ptr_iter;
            }
            ILOG("No inst in replay ");
        }

        if (!allow_speculative_load_exec_ || !win_ptr_iter.isValid())
        {
            win_ptr_iter = ldst_inst_queue_.begin();
        }

        // Select the ready instruction with highest issue priority
        for (auto iter = ldst_inst_queue_.begin(); iter != ldst_inst_queue_.end(); iter++) {
            // Skip not ready-to-issue instruction
            if (!(*iter)->isReady()) {
                continue;
            }

            // Pick winner
            if (!(*win_ptr_iter)->isReady() || (*iter)->getInstUniqueID() < (*win_ptr_iter)->getInstUniqueID()){
                win_ptr_iter = iter;
            }
            // NOTE:
            // If the inst pointed to by (*win_ptr_iter) is not ready (possible @initialization),
            // Re-assign it pointing to the ready-to-issue instruction pointed by (*iter).
            // Otherwise, both (*win_ptr_iter) and (*iter) point to ready-to-issue instructions,
            // Pick the one with higher issue priority.
        }
        sparta_assert(win_ptr_iter.isValid(), "No valid inst issued");
        sparta_assert((*win_ptr_iter)->isReady(), "Arbitration fails: no instruction is ready! " << *win_ptr_iter);

        return *win_ptr_iter;
    }

    // Check for ready to issue instructions
    bool LSU::isReadyToIssueInsts_() const
    {
        if(allow_speculative_load_exec_ && replay_buffer_.size() >= replay_buffer_size_){
            ILOG("Replay buffer is full");
            return false;
        }
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
        ILOG("Issue priority new dispatch " << inst_ptr);
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
                if(!allow_speculative_load_exec_)// Speculative misses are marked as not ready and replay event would set them back to ready
                {
                    inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                }
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::MMU_PENDING);

                // NOTE:
                // We may not have to re-activate all of the pending MMU miss instruction here
                // However, re-activation must be scheduled somewhere else

                if (inst_info_ptr->getInstPtr() == inst_ptr) {
                    // Update issue priority for this outstanding TLB miss
                    if(inst_info_ptr->getState() != LoadStoreInstInfo::IssueState::ISSUED)
                    {
                        inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                    }
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
            "Attempt to rehandle TLB lookup for instruction not yet in the issue queue! " << inst_ptr);
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
                if(!allow_speculative_load_exec_) // Speculative misses are marked as not ready and replay event would set them back to ready
                {
                    inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                }
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_PENDING);

                // NOTE:
                // We may not have to re-activate all of the pending cache miss instruction here
                // However, re-activation must be scheduled somewhere else
            }

            if (inst_info_ptr->getInstPtr() == inst_ptr) {
                // Update issue priority for this outstanding cache miss
                if(inst_info_ptr->getState() != LoadStoreInstInfo::IssueState::ISSUED)
                {
                    inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                }
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_RELOAD);

                // NOTE:
                // The priority should be set in such a way that
                // the outstanding miss is always re-issued earlier than other pending miss
                // Here we have CACHE_RELOAD > CACHE_PENDING > MMU_RELOAD

                is_found = true;
            }
        }

        sparta_assert(is_flushed_inst || is_found,
                    "Attempt to rehandle cache lookup for instruction not yet in the issue queue! " << inst_ptr);
    }

    // Update issue priority after store instruction retires
    void LSU::updateIssuePriorityAfterStoreInstRetire_(const InstPtr & inst_ptr)
    {
        for (auto &inst_info_ptr : ldst_inst_queue_) {
            if (inst_info_ptr->getInstPtr() == inst_ptr) {

                if(inst_info_ptr->getState() != LoadStoreInstInfo::IssueState::ISSUED)// Speculative misses are marked as not ready and replay event would set them back to ready
                {
                    inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                }
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_PENDING);

                return;
            }
        }

        sparta_assert(false,
            "Attempt to update issue priority for instruction not yet in the issue queue!");

    }

    bool LSU::olderStoresExists_(const InstPtr & inst_ptr)
    {
        for(const auto &ldst_inst : ldst_inst_queue_){
            const auto &ldst_inst_ptr = ldst_inst->getInstPtr();
            if(ldst_inst_ptr->isStoreInst() && ldst_inst_ptr->getUniqueID() < inst_ptr->getUniqueID()){
                return true;
            }
        }
        return false;
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
