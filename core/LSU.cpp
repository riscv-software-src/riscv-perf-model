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

    LSU::LSU(sparta::TreeNode* node, const LSUParameterSet* p) :
        sparta::Unit(node),
        ldst_inst_queue_("lsu_inst_queue", p->ldst_inst_queue_size, getClock()),
        ldst_inst_queue_size_(p->ldst_inst_queue_size),
        replay_buffer_("replay_buffer", p->replay_buffer_size, getClock()),
        replay_buffer_size_(p->replay_buffer_size),
        replay_issue_delay_(p->replay_issue_delay),
        ready_queue_(),
        load_store_info_allocator_(sparta::notNull(OlympiaAllocators::getOlympiaAllocators(node))
                                       ->load_store_info_allocator),
        memory_access_allocator_(sparta::notNull(OlympiaAllocators::getOlympiaAllocators(node))
                                     ->memory_access_allocator),
        address_calculation_stage_(0),
        mmu_lookup_stage_(address_calculation_stage_ + p->mmu_lookup_stage_length),
        cache_lookup_stage_(mmu_lookup_stage_ + p->cache_lookup_stage_length),
        cache_read_stage_(cache_lookup_stage_
                          + 1), // Get data from the cache in the cycle after cache lookup
        complete_stage_(
            cache_read_stage_
            + p->cache_read_stage_length), // Complete stage is after the cache read stage
        ldst_pipeline_("LoadStorePipeline", (complete_stage_ + 1),
                       getClock()), // complete_stage_ + 1 is number of stages
        allow_speculative_load_exec_(p->allow_speculative_load_exec)
    {
        sparta_assert(p->mmu_lookup_stage_length > 0,
                      "MMU lookup stage should atleast be one cycle");
        sparta_assert(p->cache_read_stage_length > 0,
                      "Cache read stage should atleast be one cycle");
        sparta_assert(p->cache_lookup_stage_length > 0,
                      "Cache lookup stage should atleast be one cycle");

        // Pipeline collection config
        ldst_pipeline_.enableCollection(node);
        ldst_inst_queue_.enableCollection(node);
        replay_buffer_.enableCollection(node);

        // Startup handler for sending initial credits
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(LSU, sendInitialCredits_));

        // Port config
        in_lsu_insts_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getInstsFromDispatch_, InstPtr));

        in_rob_retire_ack_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getAckFromROB_, InstPtr));

        in_reorder_flush_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(LSU, handleFlush_, FlushManager::FlushingCriteria));

        in_mmu_lookup_req_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(LSU, handleMMUReadyReq_, MemoryAccessInfoPtr));

        in_mmu_lookup_ack_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getAckFromMMU_, MemoryAccessInfoPtr));

        in_cache_lookup_req_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(LSU, handleCacheReadyReq_, MemoryAccessInfoPtr));

        in_cache_lookup_ack_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getAckFromCache_, MemoryAccessInfoPtr));

        // Allow the pipeline to create events and schedule work
        ldst_pipeline_.performOwnUpdates();

        // There can be situations where NOTHING is going on in the
        // simulator but forward progression of the pipeline elements.
        // In this case, the internal event for the LS pipeline will
        // be the only event keeping simulation alive.  Sparta
        // supports identifying non-essential events (by calling
        // setContinuing to false on any event).
        ldst_pipeline_.setContinuing(true);

        ldst_pipeline_.registerHandlerAtStage(
            address_calculation_stage_, CREATE_SPARTA_HANDLER(LSU, handleAddressCalculation_));

        ldst_pipeline_.registerHandlerAtStage(mmu_lookup_stage_,
                                              CREATE_SPARTA_HANDLER(LSU, handleMMULookupReq_));

        ldst_pipeline_.registerHandlerAtStage(cache_lookup_stage_,
                                              CREATE_SPARTA_HANDLER(LSU, handleCacheLookupReq_));

        ldst_pipeline_.registerHandlerAtStage(cache_read_stage_,
                                              CREATE_SPARTA_HANDLER(LSU, handleCacheRead_));

        ldst_pipeline_.registerHandlerAtStage(complete_stage_,
                                              CREATE_SPARTA_HANDLER(LSU, completeInst_));

        // Capture when the simulation is stopped prematurely by the ROB i.e. hitting retire limit
        node->getParent()->registerForNotification<bool, LSU, &LSU::onROBTerminate_>(
            this, "rob_stopped_notif_channel", false /* ROB maybe not be constructed yet */);

        uev_append_ready_ >> uev_issue_inst_;
        // NOTE:
        // To resolve the race condition when:
        // Both cache and MMU try to drive the single BIU port at the same cycle
        // Here we give cache the higher priority
        ILOG("LSU construct: #" << node->getGroupIdx());
    }

    LSU::~LSU()
    {
        DLOG(getContainer()->getLocation() << ": " << load_store_info_allocator_.getNumAllocated()
                                           << " LoadStoreInstInfo objects allocated/created");
        DLOG(getContainer()->getLocation() << ": " << memory_access_allocator_.getNumAllocated()
                                           << " MemoryAccessInfo objects allocated/created");
    }

    void LSU::onROBTerminate_(const bool & val) { rob_stopped_simulation_ = val; }

    void LSU::onStartingTeardown_()
    {
        // If ROB has not stopped the simulation &
        // the ldst has entries to process we should fail
        if ((false == rob_stopped_simulation_) && (false == ldst_inst_queue_.empty()))
        {
            dumpDebugContent_(std::cerr);
            sparta_assert(false, "Issue queue has pending instructions");
        }
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
    void LSU::setupScoreboard_()
    {
        // Setup scoreboard view upon register file
        std::vector<core_types::RegFile> reg_files = {core_types::RF_INTEGER, core_types::RF_FLOAT};
        // if we ever move to multicore, we only want to have resources look for scoreboard in their cpu
        // if we're running a test where we only have top.rename or top.issue_queue, then we can just use the root
        auto cpu_node = getContainer()->findAncestorByName("core.*");
        if(cpu_node == nullptr){
            cpu_node = getContainer()->getRoot();
        }
        for (const auto rf : reg_files)
        {
            scoreboard_views_[rf].reset(new sparta::ScoreboardView(
                getContainer()->getName(), core_types::regfile_names[rf], cpu_node));
        }
    }

    // Receive new load/store instruction from Dispatch Unit
    void LSU::getInstsFromDispatch_(const InstPtr & inst_ptr)
    {
        ILOG("New instruction added to the ldst queue " << inst_ptr);
        allocateInstToIssueQueue_(inst_ptr);
        handleOperandIssueCheck_(inst_ptr);
        lsu_insts_dispatched_++;
    }

    // Callback from Scoreboard to inform Operand Readiness
    void LSU::handleOperandIssueCheck_(const InstPtr & inst_ptr)
    {
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
                [this, inst_ptr](const sparta::Scoreboard::RegisterBitMask &)
                { this->handleOperandIssueCheck_(inst_ptr); });
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
                            [this, inst_ptr](const sparta::Scoreboard::RegisterBitMask &)
                            { this->handleOperandIssueCheck_(inst_ptr); });
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

            appendToReadyQueue_(inst_ptr);

            // NOTE:
            // It is a bug if instruction status is updated as SCHEDULED in the issueInst_()
            // The reason is: when issueInst_() is called, it could be scheduled for
            // either a new issue event, or a re-issue event
            // however, we can ONLY update instruction status as SCHEDULED for a new issue event

            ILOG("Another issue event scheduled " << inst_ptr);

            if (isReadyToIssueInsts_())
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
        if (isReadyToIssueInsts_())
        {
            ILOG("ROB Ack issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }

        ILOG("ROB Ack: Retired store instruction: " << inst_ptr);
    }

    // Issue/Re-issue ready instructions in the issue queue
    void LSU::issueInst_()
    {
        // Instruction issue arbitration
        const LoadStoreInstInfoPtr win_ptr = arbitrateInstIssue_();
        // NOTE:
        // win_ptr should always point to an instruction ready to be issued
        // Otherwise assertion error should already be fired in arbitrateInstIssue_()
        ILOG("Arbitrated inst " << win_ptr << " " << win_ptr->getInstPtr());

        ++lsu_insts_issued_;

        // Append load/store pipe
        ldst_pipeline_.append(win_ptr);

        // We append to replay queue to prevent ref count of the shared pointer to drop before
        // calling pop below
        if (allow_speculative_load_exec_)
        {
            ILOG("Appending to replay queue " << win_ptr);
            appendToReplayQueue_(win_ptr);
        }

        // Remove inst from ready queue
        win_ptr->setInReadyQueue(false);

        // Update instruction issue info
        win_ptr->setState(LoadStoreInstInfo::IssueState::ISSUED);
        win_ptr->setPriority(LoadStoreInstInfo::IssuePriority::LOWEST);

        // Schedule another instruction issue event if possible
        if (isReadyToIssueInsts_())
        {
            ILOG("IssueInst_ issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(1));
        }
    }

    void LSU::handleAddressCalculation_()
    {
        auto stage_id = address_calculation_stage_;

        if (!ldst_pipeline_.isValid(stage_id))
        {
            return;
        }

        auto & ldst_info_ptr = ldst_pipeline_[stage_id];
        auto & inst_ptr = ldst_info_ptr->getInstPtr();
        // Assume Calculate Address

        ILOG("Address Generation " << inst_ptr << ldst_info_ptr);
        if (isReadyToIssueInsts_())
        {
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // MMU subroutines
    ////////////////////////////////////////////////////////////////////////////////
    // Handle MMU access request
    void LSU::handleMMULookupReq_()
    {
        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(mmu_lookup_stage_))
        {
            return;
        }

        const LoadStoreInstInfoPtr & load_store_info_ptr = ldst_pipeline_[mmu_lookup_stage_];
        const MemoryAccessInfoPtr & mem_access_info_ptr =
            load_store_info_ptr->getMemoryAccessInfoPtr();
        const InstPtr & inst_ptr = load_store_info_ptr->getInstPtr();

        const bool mmu_bypass =
            (mem_access_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::HIT);

        if (mmu_bypass)
        {
            ILOG("MMU Lookup is skipped (TLB is already hit)! " << load_store_info_ptr);
            return;
        }

        // Ready dependent younger loads
        if (false == allow_speculative_load_exec_)
        {
            if (inst_ptr->isStoreInst())
            {
                readyDependentLoads_(load_store_info_ptr);
            }
        }

        out_mmu_lookup_req_.send(mem_access_info_ptr);
        ILOG(mem_access_info_ptr << load_store_info_ptr);
    }

    void LSU::getAckFromMMU_(const MemoryAccessInfoPtr & updated_memory_access_info_ptr)
    {
        const auto stage_id = mmu_lookup_stage_;

        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(stage_id))
        {
            ILOG("MMU stage not valid");
            return;
        }
        ILOG("MMU Ack: " << std::boolalpha << updated_memory_access_info_ptr->getPhyAddrStatus()
                         << " " << updated_memory_access_info_ptr);
        const bool mmu_hit_ = updated_memory_access_info_ptr->getPhyAddrStatus();

        if (updated_memory_access_info_ptr->getInstPtr()->isStoreInst() && mmu_hit_
            && allow_speculative_load_exec_)
        {
            ILOG("Aborting speculative loads " << updated_memory_access_info_ptr);
            abortYoungerLoads_(updated_memory_access_info_ptr);
        }
    }

    void LSU::handleMMUReadyReq_(const MemoryAccessInfoPtr & memory_access_info_ptr)
    {
        ILOG("MMU rehandling event is scheduled! " << memory_access_info_ptr);
        const auto & inst_ptr = memory_access_info_ptr->getInstPtr();

        // Update issue priority & Schedule an instruction (re-)issue event
        updateIssuePriorityAfterTLBReload_(memory_access_info_ptr);

        if (inst_ptr->getFlushedStatus())
        {
            if (isReadyToIssueInsts_())
            {
                uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
            }
            return;
        }

        removeInstFromReplayQueue_(inst_ptr);

        if (isReadyToIssueInsts_())
        {
            ILOG("MMU ready issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Cache Subroutine
    ////////////////////////////////////////////////////////////////////////////////
    // Handle cache access request
    void LSU::handleCacheLookupReq_()
    {
        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(cache_lookup_stage_))
        {
            return;
        }

        const LoadStoreInstInfoPtr & load_store_info_ptr = ldst_pipeline_[cache_lookup_stage_];
        const MemoryAccessInfoPtr & mem_access_info_ptr =
            load_store_info_ptr->getMemoryAccessInfoPtr();
        const bool phy_addr_is_ready = mem_access_info_ptr->getPhyAddrStatus();

        // If we did not have an MMU hit from previous stage, invalidate and bail
        if (false == phy_addr_is_ready)
        {
            ILOG("Cache Lookup is skipped (Physical address not ready)!" << load_store_info_ptr);
            if (allow_speculative_load_exec_)
            {
                updateInstReplayReady_(load_store_info_ptr);
            }
            // There might not be a wake up because the cache cannot handle nay more instruction
            // Change to nack wakeup when implemented
            if (!load_store_info_ptr->isInReadyQueue())
            {
                appendToReadyQueue_(load_store_info_ptr);
                load_store_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                if (isReadyToIssueInsts_())
                {
                    uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
                }
            }
            ldst_pipeline_.invalidateStage(cache_lookup_stage_);
            return;
        }

        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        ILOG(load_store_info_ptr << " " << mem_access_info_ptr);

        // If have passed translation and the instruction is a store,
        // then it's good to be retired (i.e. mark it completed).
        // Stores typically do not cause a flush after a successful
        // translation.  We now wait for the Retire block to "retire"
        // it, meaning it's good to go to the cache
        if (inst_ptr->isStoreInst() && (inst_ptr->getStatus() == Inst::Status::SCHEDULED))
        {
            ILOG("Store marked as completed " << inst_ptr);
            inst_ptr->setStatus(Inst::Status::COMPLETED);
            load_store_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
            ldst_pipeline_.invalidateStage(cache_lookup_stage_);
            if (allow_speculative_load_exec_)
            {
                updateInstReplayReady_(load_store_info_ptr);
            }
            return;
        }

        // Loads dont perform a cache lookup if there are older stores present in the load store
        // queue
        if (!inst_ptr->isStoreInst() && olderStoresExists_(inst_ptr)
            && allow_speculative_load_exec_)
        {
            ILOG("Dropping speculative load " << inst_ptr);
            load_store_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
            ldst_pipeline_.invalidateStage(cache_lookup_stage_);
            if (allow_speculative_load_exec_)
            {
                updateInstReplayReady_(load_store_info_ptr);
            }
            return;
        }

        const bool is_already_hit =
            (mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT);
        const bool is_unretired_store =
            inst_ptr->isStoreInst() && (inst_ptr->getStatus() != Inst::Status::RETIRED);
        const bool cache_bypass = is_already_hit || !phy_addr_is_ready || is_unretired_store;

        if (cache_bypass)
        {
            if (is_already_hit)
            {
                ILOG("Cache Lookup is skipped (Cache already hit)");
            }
            else if (is_unretired_store)
            {
                ILOG("Cache Lookup is skipped (store instruction not oldest)");
            }
            else
            {
                sparta_assert(false, "Cache access is bypassed without a valid reason!");
            }
            return;
        }

        out_cache_lookup_req_.send(mem_access_info_ptr);
    }

    void LSU::getAckFromCache_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const LoadStoreInstIterator & iter = mem_access_info_ptr->getIssueQueueIterator();
        if (!iter.isValid())
        {
            return;
        }

        // Is its a cache miss we dont need to rechedule the instruction
        if (!mem_access_info_ptr->isCacheHit())
        {
            return;
        }

        const LoadStoreInstInfoPtr & inst_info_ptr = *(iter);

        // Update issue priority for this outstanding cache miss
        if (inst_info_ptr->getState() != LoadStoreInstInfo::IssueState::ISSUED)
        {
            inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
        }

        inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_RELOAD);
        if (!inst_info_ptr->isInReadyQueue())
        {
            uev_append_ready_.preparePayload(inst_info_ptr)->schedule(sparta::Clock::Cycle(0));
        }
    }

    void LSU::handleCacheReadyReq_(const MemoryAccessInfoPtr & memory_access_info_ptr)
    {
        auto inst_ptr = memory_access_info_ptr->getInstPtr();
        if (inst_ptr->getFlushedStatus())
        {
            ILOG("BIU Ack for a flushed cache miss is received!");

            // Schedule an instruction (re-)issue event
            // Note: some younger load/store instruction(s) might have been blocked by
            // this outstanding miss
            if (isReadyToIssueInsts_())
            {
                uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
            }

            return;
        }

        ILOG("Cache ready for " << memory_access_info_ptr);
        updateIssuePriorityAfterCacheReload_(memory_access_info_ptr);
        removeInstFromReplayQueue_(inst_ptr);

        if (isReadyToIssueInsts_())
        {
            ILOG("Cache ready issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    void LSU::handleCacheRead_()
    {
        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(cache_read_stage_))
        {
            return;
        }

        const LoadStoreInstInfoPtr & load_store_info_ptr = ldst_pipeline_[cache_read_stage_];
        const MemoryAccessInfoPtr & mem_access_info_ptr =
            load_store_info_ptr->getMemoryAccessInfoPtr();
        ILOG(mem_access_info_ptr);

        if (false == mem_access_info_ptr->isCacheHit())
        {
            ILOG("Cannot complete inst, cache miss: " << mem_access_info_ptr);
            if (allow_speculative_load_exec_)
            {
                updateInstReplayReady_(load_store_info_ptr);
            }
            // There might not be a wake up because the cache cannot handle nay more instruction
            // Change to nack wakeup when implemented
            if (!load_store_info_ptr->isInReadyQueue())
            {
                appendToReadyQueue_(load_store_info_ptr);
                load_store_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                if (isReadyToIssueInsts_())
                {
                    uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
                }
            }
            ldst_pipeline_.invalidateStage(cache_read_stage_);
            return;
        }

        if (mem_access_info_ptr->isDataReady())
        {
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
        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(complete_stage_))
        {
            return;
        }

        const LoadStoreInstInfoPtr & load_store_info_ptr = ldst_pipeline_[complete_stage_];
        const MemoryAccessInfoPtr & mem_access_info_ptr =
            load_store_info_ptr->getMemoryAccessInfoPtr();

        if (false == mem_access_info_ptr->isDataReady())
        {
            ILOG("Cannot complete inst, cache data is missing: " << mem_access_info_ptr);
            return;
        }

        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        const bool is_store_inst = inst_ptr->isStoreInst();
        ILOG("Completing inst: " << inst_ptr);
        ILOG(mem_access_info_ptr);

        core_types::RegFile reg_file = core_types::RF_INTEGER;
        const auto & dests = inst_ptr->getDestOpInfoList();
        if (dests.size() > 0)
        {
            sparta_assert(dests.size() == 1); // we should only have one destination
            reg_file = olympia::coreutils::determineRegisterFile(dests[0]);
            const auto & dest_bits = inst_ptr->getDestRegisterBitMask(reg_file);
            scoreboard_views_[reg_file]->setReady(dest_bits);
        }

        // Complete load instruction
        if (!is_store_inst)
        {
            sparta_assert(mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT,
                          "Load instruction cannot complete when cache is still a miss! "
                              << mem_access_info_ptr);

            if (isReadyToIssueInsts_())
            {
                ILOG("Complete issue");
                uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
            }
            if (load_store_info_ptr->isRetired()
                || inst_ptr->getStatus() == Inst::Status::COMPLETED)
            {
                ILOG("Load was previously completed or retired " << load_store_info_ptr);
                if (allow_speculative_load_exec_)
                {
                    ILOG("Removed replay " << inst_ptr);
                    removeInstFromReplayQueue_(load_store_info_ptr);
                }
                return;
            }

            // Mark instruction as completed
            inst_ptr->setStatus(Inst::Status::COMPLETED);

            // Remove completed instruction from queues
            ILOG("Removed issue queue " << inst_ptr);
            popIssueQueue_(load_store_info_ptr);

            if (allow_speculative_load_exec_)
            {
                ILOG("Removed replay " << inst_ptr);
                removeInstFromReplayQueue_(load_store_info_ptr);
            }

            lsu_insts_completed_++;
            out_lsu_credits_.send(1, 0);

            ILOG("Complete Load Instruction: " << inst_ptr->getMnemonic() << " uid("
                                               << inst_ptr->getUniqueID() << ")");

            return;
        }

        // Complete store instruction
        if (inst_ptr->getStatus() != Inst::Status::RETIRED)
        {

            sparta_assert(mem_access_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::HIT,
                          "Store instruction cannot complete when TLB is still a miss!");

            ILOG("Store was completed but waiting for retire " << load_store_info_ptr);

            if (isReadyToIssueInsts_())
            {
                ILOG("Store complete issue");
                uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
            }
        }
        // Finish store operation
        else
        {
            sparta_assert(mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT,
                          "Store inst cannot finish when cache is still a miss! " << inst_ptr);

            sparta_assert(mem_access_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::HIT,
                          "Store inst cannot finish when cache is still a miss! " << inst_ptr);
            if (isReadyToIssueInsts_())
            {
                ILOG("Complete store issue");
                uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
            }

            if (!load_store_info_ptr->getIssueQueueIterator().isValid())
            {
                ILOG("Inst was already retired " << load_store_info_ptr);
                if (allow_speculative_load_exec_)
                {
                    ILOG("Removed replay " << load_store_info_ptr);
                    removeInstFromReplayQueue_(load_store_info_ptr);
                }
                return;
            }

            ILOG("Removed issue queue " << inst_ptr);
            popIssueQueue_(load_store_info_ptr);

            if (allow_speculative_load_exec_)
            {
                ILOG("Removed replay " << load_store_info_ptr);
                removeInstFromReplayQueue_(load_store_info_ptr);
            }

            lsu_insts_completed_++;
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

        lsu_flushes_++;

        // Flush load/store pipeline entry
        flushLSPipeline_(criteria);

        // Flush instruction issue queue
        flushIssueQueue_(criteria);
        flushReplayBuffer_(criteria);
        flushReadyQueue_(criteria);

        // Cancel replay events
        auto flush = [&criteria](const LoadStoreInstInfoPtr & ldst_info_ptr) -> bool {
            return criteria.includedInFlush(ldst_info_ptr->getInstPtr());
        };
        uev_append_ready_.cancelIf(flush);
        uev_replay_ready_.cancelIf(flush);

        // Cancel issue event already scheduled if no ready-to-issue inst left after flush
        if (!isReadyToIssueInsts_())
        {
            uev_issue_inst_.cancel();
        }

        // NOTE:
        // Flush is handled at Flush phase (inbetween PortUpdate phase and Tick phase).
        // This also guarantees that whenever an instruction issue event happens,
        // instruction issue arbitration should always succeed, even when flush happens.
        // Otherwise, assertion error is fired inside arbitrateInstIssue_()
    }

    void LSU::dumpDebugContent_(std::ostream & output) const
    {
        output << "LSU Contents" << std::endl;
        for (const auto & entry : ldst_inst_queue_)
        {
            output << '\t' << entry << std::endl;
        }
    }

    void LSU::replayReady_(const LoadStoreInstInfoPtr & replay_inst_ptr)
    {
        ILOG("Replay inst ready " << replay_inst_ptr);
        // We check in the ldst_queue as the instruction may not be in the replay queue
        if (replay_inst_ptr->getState() == LoadStoreInstInfo::IssueState::NOT_READY)
        {
            replay_inst_ptr->setState(LoadStoreInstInfo::IssueState::READY);
        }
        auto issue_priority = replay_inst_ptr->getMemoryAccessInfoPtr()->getPhyAddrStatus()
                                  ? LoadStoreInstInfo::IssuePriority::CACHE_PENDING
                                  : LoadStoreInstInfo::IssuePriority::MMU_PENDING;
        replay_inst_ptr->setPriority(issue_priority);
        uev_append_ready_.preparePayload(replay_inst_ptr)->schedule(sparta::Clock::Cycle(0));

        if (isReadyToIssueInsts_())
        {
            ILOG("replay ready issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    void LSU::updateInstReplayReady_(const LoadStoreInstInfoPtr & load_store_info_ptr)
    {
        ILOG("Scheduled replay " << load_store_info_ptr << " after " << replay_issue_delay_
                                 << " cycles");
        load_store_info_ptr->setState(LoadStoreInstInfo::IssueState::NOT_READY);
        uev_replay_ready_.preparePayload(load_store_info_ptr)
            ->schedule(sparta::Clock::Cycle(replay_issue_delay_));
        removeInstFromReplayQueue_(load_store_info_ptr);

        replay_insts_++;
    }

    void LSU::appendReady_(const LoadStoreInstInfoPtr & replay_inst_ptr)
    {
        ILOG("Appending to Ready ready queue event " << replay_inst_ptr->isInReadyQueue() << " "
                                                     << replay_inst_ptr);
        if (!replay_inst_ptr->isInReadyQueue()
            && !replay_inst_ptr->getReplayQueueIterator().isValid())
            appendToReadyQueue_(replay_inst_ptr);
        if (isReadyToIssueInsts_())
        {
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Regular Function/Subroutine Call
    ////////////////////////////////////////////////////////////////////////////////
    LSU::LoadStoreInstInfoPtr LSU::createLoadStoreInst_(const InstPtr & inst_ptr)
    {
        // Create load/store memory access info
        MemoryAccessInfoPtr mem_info_ptr = sparta::allocate_sparta_shared_pointer<MemoryAccessInfo>(
            memory_access_allocator_, inst_ptr);
        // Create load/store instruction issue info
        LoadStoreInstInfoPtr inst_info_ptr =
            sparta::allocate_sparta_shared_pointer<LoadStoreInstInfo>(load_store_info_allocator_,
                                                                      mem_info_ptr);
        return inst_info_ptr;
    }

    void LSU::allocateInstToIssueQueue_(const InstPtr & inst_ptr)
    {
        auto inst_info_ptr = createLoadStoreInst_(inst_ptr);

        sparta_assert(ldst_inst_queue_.size() < ldst_inst_queue_size_,
                      "Appending issue queue causes overflows!");

        // Always append newly dispatched instructions to the back of issue queue
        const LoadStoreInstIterator & iter = ldst_inst_queue_.push_back(inst_info_ptr);
        inst_info_ptr->setIssueQueueIterator(iter);

        ILOG("Append new load/store instruction to issue queue!");
    }

    bool LSU::allOlderStoresIssued_(const InstPtr & inst_ptr)
    {
        for (const auto & ldst_info_ptr : ldst_inst_queue_)
        {
            const auto & ldst_inst_ptr = ldst_info_ptr->getInstPtr();
            const auto & mem_info_ptr = ldst_info_ptr->getMemoryAccessInfoPtr();
            if (ldst_inst_ptr->isStoreInst()
                && ldst_inst_ptr->getUniqueID() < inst_ptr->getUniqueID()
                && !mem_info_ptr->getPhyAddrStatus() && ldst_info_ptr->getInstPtr() != inst_ptr)
            {
                return false;
            }
        }
        return true;
    }

    // Only called if allow_spec_load_exec is true
    void LSU::readyDependentLoads_(const LoadStoreInstInfoPtr & store_inst_ptr)
    {
        bool found = false;
        for (auto & ldst_inst_ptr : ldst_inst_queue_)
        {
            auto & inst_ptr = ldst_inst_ptr->getInstPtr();
            if (inst_ptr->isStoreInst())
            {
                continue;
            }

            // Only ready loads which have register operands ready
            // We only care of the instructions which are still not ready
            // Instruction have a status of SCHEDULED if they are ready to be issued
            if (inst_ptr->getStatus() == Inst::Status::DISPATCHED && instOperandReady_(inst_ptr))
            {
                ILOG("Updating inst to schedule " << inst_ptr << " " << ldst_inst_ptr);
                updateIssuePriorityAfterNewDispatch_(inst_ptr);
                appendToReadyQueue_(ldst_inst_ptr);
                found = true;
            }
        }

        if (found && isReadyToIssueInsts_())
        {
            ILOG("Ready dep inst issue ");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    bool LSU::instOperandReady_(const InstPtr & inst_ptr)
    {
        return scoreboard_views_[core_types::RF_INTEGER]->isSet(
            inst_ptr->getSrcRegisterBitMask(core_types::RF_INTEGER));
    }

    void LSU::abortYoungerLoads_(const olympia::MemoryAccessInfoPtr & memory_access_info_ptr)
    {
        auto & inst_ptr = memory_access_info_ptr->getInstPtr();
        uint64_t min_inst_age = UINT64_MAX;
        // Find oldest instruction age with the same Virtual address
        for (auto iter = replay_buffer_.begin(); iter != replay_buffer_.end(); iter++)
        {
            auto & queue_inst = (*iter)->getInstPtr();
            //  Skip stores or the instruction being compared against
            if (queue_inst->isStoreInst() || queue_inst == inst_ptr)
            {
                continue;
            }
            // Find loads which have the same address
            // Record the oldest age to abort instructions younger than it
            if (queue_inst->getTargetVAddr() == inst_ptr->getTargetVAddr()
                && queue_inst->getUniqueID() < min_inst_age)
            {
                min_inst_age = queue_inst->getUniqueID();
            }
        }

        if (min_inst_age == UINT64_MAX)
        {
            ILOG("No younger instruction to deallocate");
            return;
        }

        ILOG("Age of the oldest instruction " << min_inst_age << " for " << inst_ptr
                                              << inst_ptr->getTargetVAddr());

        // Remove instructions younger than the oldest load that was removed
        auto iter = replay_buffer_.begin();
        while (iter != replay_buffer_.end())
        {
            auto replay_inst_iter(iter++);
            auto & replay_inst = *replay_inst_iter;
            // Apply to loads only
            if (replay_inst->getInstPtr()->isStoreInst())
            {
                continue;
            }

            if (replay_inst->getInstUniqueID() >= min_inst_age)
            {
                (replay_inst)->setState(LoadStoreInstInfo::IssueState::READY);
                appendToReadyQueue_(replay_inst);

                ILOG("Aborted younger load "
                     << replay_inst << replay_inst->getInstPtr()->getTargetVAddr() << inst_ptr);
                dropInstFromPipeline_(replay_inst);
                removeInstFromReplayQueue_(replay_inst);
            }
        }
    }

    // Drop instruction from the pipeline
    // Pipeline stages might be multi cycle hence we have check all the stages
    void LSU::dropInstFromPipeline_(const LoadStoreInstInfoPtr & load_store_inst_info_ptr)
    {
        ILOG("Dropping instruction from pipeline " << load_store_inst_info_ptr);

        for (int stage = 0; stage <= complete_stage_; stage++)
        {
            if (ldst_pipeline_.isValid(stage))
            {
                const auto & pipeline_inst = ldst_pipeline_[stage];
                if (pipeline_inst == load_store_inst_info_ptr)
                {
                    ldst_pipeline_.invalidateStage(stage);
                    return;
                }
            }
        }
    }

    void LSU::removeInstFromReplayQueue_(const InstPtr & inst_to_remove)
    {
        ILOG("Removing Inst from replay queue " << inst_to_remove);
        for (const auto & ldst_inst : ldst_inst_queue_)
        {
            if (ldst_inst->getInstPtr() == inst_to_remove)
            {
                if (ldst_inst->getReplayQueueIterator().isValid())
                {
                    removeInstFromReplayQueue_(ldst_inst);
                }
                else
                {
                    // Handle situations when replay delay completes before mmu/cache is ready
                    ILOG("Invalid Replay queue entry " << inst_to_remove);
                }
            }
        }
    }

    void LSU::removeInstFromReplayQueue_(const LoadStoreInstInfoPtr & inst_to_remove)
    {
        ILOG("Removing Inst from replay queue " << inst_to_remove);
        if (inst_to_remove->getReplayQueueIterator().isValid())
            replay_buffer_.erase(inst_to_remove->getReplayQueueIterator());
        // Invalidate the iterator manually
        inst_to_remove->setReplayQueueIterator(LoadStoreInstIterator());
    }

    // Pop completed load/store instruction out of issue queue
    void LSU::popIssueQueue_(const LoadStoreInstInfoPtr & inst_ptr)
    {
        ILOG("Removing Inst from issue queue " << inst_ptr);
        ldst_inst_queue_.erase(inst_ptr->getIssueQueueIterator());
        // Invalidate the iterator manually
        inst_ptr->setIssueQueueIterator(LoadStoreInstIterator());
    }

    void LSU::appendToReplayQueue_(const LoadStoreInstInfoPtr & inst_info_ptr)
    {
        sparta_assert(replay_buffer_.size() < replay_buffer_size_,
                      "Appending load queue causes overflows!");

        const bool iter_exists = inst_info_ptr->getReplayQueueIterator().isValid();
        sparta_assert(!iter_exists,
                      "Cannot push duplicate instructions into the replay queue " << inst_info_ptr);

        // Always append newly dispatched instructions to the back of issue queue
        const auto & iter = replay_buffer_.push_back(inst_info_ptr);
        inst_info_ptr->setReplayQueueIterator(iter);

        ILOG("Append new instruction to replay queue!" << inst_info_ptr);
    }

    void LSU::appendToReadyQueue_(const InstPtr & inst_ptr)
    {
        for (const auto & inst : ldst_inst_queue_)
        {
            if (inst_ptr == inst->getInstPtr())
            {
                appendToReadyQueue_(inst);
                return;
            }
        }

        sparta_assert(false, "Instruction not found in the issue queue " << inst_ptr);
    }

    void LSU::appendToReadyQueue_(const LoadStoreInstInfoPtr & ldst_inst_ptr)
    {
        ILOG("Appending to Ready queue " << ldst_inst_ptr);
        for (const auto & inst : ready_queue_)
        {
            sparta_assert(inst != ldst_inst_ptr, "Instruction in ready queue " << ldst_inst_ptr);
        }
        ready_queue_.insert(ldst_inst_ptr);
        ldst_inst_ptr->setInReadyQueue(true);
        ldst_inst_ptr->setState(LoadStoreInstInfo::IssueState::READY);
    }

    // Arbitrate instruction issue from ldst_inst_queue
    LSU::LoadStoreInstInfoPtr LSU::arbitrateInstIssue_()
    {
        sparta_assert(ready_queue_.size() > 0, "Arbitration fails: issue is empty!");

        LoadStoreInstInfoPtr ready_inst_ = ready_queue_.top();
        ready_queue_.pop();

        return ready_inst_;
    }

    // Check for ready to issue instructions
    bool LSU::isReadyToIssueInsts_() const
    {
        if (allow_speculative_load_exec_ && replay_buffer_.size() >= replay_buffer_size_)
        {
            ILOG("Replay buffer is full");
            return false;
        }

        if (!ready_queue_.empty())
        {
            return true;
        }

        ILOG("No instructions are ready to be issued");

        return false;
    }

    // Update issue priority when newly dispatched instruction comes in
    void LSU::updateIssuePriorityAfterNewDispatch_(const InstPtr & inst_ptr)
    {
        ILOG("Issue priority new dispatch " << inst_ptr);
        for (auto & inst_info_ptr : ldst_inst_queue_)
        {
            if (inst_info_ptr->getInstPtr() == inst_ptr)
            {
                inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::NEW_DISP);
                // NOTE:
                // IssuePriority should always be updated before a new issue event is scheduled.
                // This guarantees that whenever a new instruction issue event is scheduled:
                // (1)Instruction issue queue already has "something READY";
                // (2)Instruction issue arbitration is guaranteed to be sucessful.

                // Update instruction status
                inst_ptr->setStatus(Inst::Status::SCHEDULED);
                return;
            }
        }

        sparta_assert(
            false, "Attempt to update issue priority for instruction not yet in the issue queue!");
    }

    // Update issue priority after tlb reload
    void LSU::updateIssuePriorityAfterTLBReload_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        bool is_found = false;
        for (auto & inst_info_ptr : ldst_inst_queue_)
        {
            const MemoryAccessInfoPtr & mem_info_ptr = inst_info_ptr->getMemoryAccessInfoPtr();
            if (mem_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::MISS)
            {
                // Re-activate all TLB-miss-pending instructions in the issue queue
                if (!allow_speculative_load_exec_) // Speculative misses are marked as not ready and
                                                   // replay event would set them back to ready
                {
                    inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                }
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::MMU_PENDING);
            }
            // NOTE:
            // We may not have to re-activate all of the pending MMU miss instruction here
            // However, re-activation must be scheduled somewhere else

            if (inst_info_ptr->getInstPtr() == inst_ptr)
            {
                // Update issue priority for this outstanding TLB miss
                if (inst_info_ptr->getState() != LoadStoreInstInfo::IssueState::ISSUED)
                {
                    inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                }
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::MMU_RELOAD);
                uev_append_ready_.preparePayload(inst_info_ptr)->schedule(sparta::Clock::Cycle(0));

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
    void LSU::updateIssuePriorityAfterCacheReload_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();

        sparta_assert(
            inst_ptr->getFlushedStatus() == false,
            "Attempt to rehandle cache lookup for flushed instruction!");

        const LoadStoreInstIterator & iter = mem_access_info_ptr->getIssueQueueIterator();
        sparta_assert(
            iter.isValid(),
            "Attempt to rehandle cache lookup for instruction not yet in the issue queue! "
                << mem_access_info_ptr);

        const LoadStoreInstInfoPtr & inst_info_ptr = *(iter);

        // Update issue priority for this outstanding cache miss
        if (inst_info_ptr->getState() != LoadStoreInstInfo::IssueState::ISSUED)
        {
            inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
        }
        inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_RELOAD);
        uev_append_ready_.preparePayload(inst_info_ptr)->schedule(sparta::Clock::Cycle(0));
    }

    // Update issue priority after store instruction retires
    void LSU::updateIssuePriorityAfterStoreInstRetire_(const InstPtr & inst_ptr)
    {
        for (auto & inst_info_ptr : ldst_inst_queue_)
        {
            if (inst_info_ptr->getInstPtr() == inst_ptr)
            {

                if (inst_info_ptr->getState()
                    != LoadStoreInstInfo::IssueState::ISSUED) // Speculative misses are marked as
                                                              // not ready and replay event would
                                                              // set them back to ready
                {
                    inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                }
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_PENDING);
                uev_append_ready_.preparePayload(inst_info_ptr)->schedule(sparta::Clock::Cycle(0));

                return;
            }
        }

        sparta_assert(
            false, "Attempt to update issue priority for instruction not yet in the issue queue!");
    }

    bool LSU::olderStoresExists_(const InstPtr & inst_ptr)
    {
        for (const auto & ldst_inst : ldst_inst_queue_)
        {
            const auto & ldst_inst_ptr = ldst_inst->getInstPtr();
            if (ldst_inst_ptr->isStoreInst()
                && ldst_inst_ptr->getUniqueID() < inst_ptr->getUniqueID())
            {
                return true;
            }
        }
        return false;
    }

    // Flush instruction issue queue
    void LSU::flushIssueQueue_(const FlushCriteria & criteria)
    {
        uint32_t credits_to_send = 0;

        auto iter = ldst_inst_queue_.begin();
        while (iter != ldst_inst_queue_.end()) {
            auto inst_ptr = (*iter)->getInstPtr();

            auto delete_iter = iter++;

            if (criteria.includedInFlush(inst_ptr)) {
                ldst_inst_queue_.erase(delete_iter);

                // Clear any scoreboard callback
                std::vector<core_types::RegFile> reg_files = {core_types::RF_INTEGER, core_types::RF_FLOAT};
                for(const auto rf : reg_files)
                {
                    scoreboard_views_[rf]->clearCallbacks(inst_ptr->getUniqueID());
                }

                // NOTE:
                // We cannot increment iter after erase because it's already invalidated by then

                ++credits_to_send;

                ILOG("Flush Instruction ID: " << inst_ptr->getUniqueID());
            }
        }

        if (credits_to_send > 0)
        {
            out_lsu_credits_.send(credits_to_send);

            ILOG("Flush " << credits_to_send << " instructions in issue queue!");
        }
    }

    // Flush load/store pipe
    void LSU::flushLSPipeline_(const FlushCriteria & criteria)
    {
        uint32_t stage_id = 0;
        for (auto iter = ldst_pipeline_.begin(); iter != ldst_pipeline_.end(); iter++, stage_id++) {
            // If the pipe stage is already invalid, no need to criteria
            if (!iter.isValid()) {
                continue;
            }

            auto inst_ptr = (*iter)->getInstPtr();
            if (criteria.includedInFlush(inst_ptr)) {
                ldst_pipeline_.flushStage(iter);

                ILOG("Flush Pipeline Stage[" << stage_id
                     << "], Instruction ID: " << inst_ptr->getUniqueID());
            }
        }
    }

    void LSU::flushReadyQueue_(const FlushCriteria & criteria)
    {
        auto iter = ready_queue_.begin();
        while (iter != ready_queue_.end()) {
            auto inst_ptr = (*iter)->getInstPtr();

            auto delete_iter = iter++;

            if (criteria.includedInFlush(inst_ptr)) {
                ready_queue_.erase(delete_iter);
                ILOG("Flushing from ready queue - Instruction ID: " << inst_ptr->getUniqueID());
            }
        }
    }

    void LSU::flushReplayBuffer_(const FlushCriteria & criteria)
    {
        auto iter = replay_buffer_.begin();
        while (iter != replay_buffer_.end()) {
            auto inst_ptr = (*iter)->getInstPtr();

            auto delete_iter = iter++;

            if (criteria.includedInFlush(inst_ptr)) {
                replay_buffer_.erase(delete_iter);
                ILOG("Flushing from replay buffer - Instruction ID: " << inst_ptr->getUniqueID());
            }
        }
    }

} // namespace olympia
