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
        num_pipelines_(p->num_pipelines),
        reservation_station_size_(p->reservation_station_size),
        reservation_station_("reservation_station", p->reservation_station_size, node->getClock()),
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
        // ldst_pipeline_("LoadStorePipeline", (complete_stage_ + 1),
        //                getClock()), // complete_stage_ + 1 is number of stages
        allow_speculative_load_exec_(p->allow_speculative_load_exec)
    {
        sparta_assert(p->mmu_lookup_stage_length > 0,
                      "MMU lookup stage should atleast be one cycle");
        sparta_assert(p->cache_read_stage_length > 0,
                      "Cache read stage should atleast be one cycle");
        sparta_assert(p->cache_lookup_stage_length > 0,
                      "Cache lookup stage should atleast be one cycle");

        // Initialize multiple pipelines
        for (uint32_t i = 0; i < num_pipelines_; ++i) {
            lsu_pipelines.emplace_back("LSU_Pipeline_" + std::to_string(i), complete_stage_ + 1, getClock());
            lsu_pipelines.back().enableCollection(node);
        }

        // Reservation station collection config
        reservation_station_.enableCollection(node);
        // // Pipeline collection config
        // ldst_pipeline_.enableCollection(node);
        // ldst_inst_queue_.enableCollection(node);
        replay_buffer_.enableCollection(node);

        // Startup handler for sending initial credits
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(LSU, sendInitialCredits_));

        // Port config
        // in_lsu_insts_.registerConsumerHandler(
        //     CREATE_SPARTA_HANDLER_WITH_DATA(LSU, getInstsFromDispatch_, InstPtr));
        in_inst_dispatch_.registerConsumerHandler_(
            CREATE_SPARTA_HANDLER_WITH_DATA(LSU, receiveInst_, InstPtr));

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

        // Configure multiple pipelines
        for (auto& pipeline : lsu_pipelines){
            pipeline.performOwnUpdates();
            pipeline.setContinuing(True);

            pipeline.registerHandlerAtStage(
                address_calculation_stage_, CREATE_SPARTA_HANDLER(LSU, handleAddressCalculation_));

            pipeline.registerHandlerAtStage(mmu_lookup_stage_,
                                            CREATE_SPARTA_HANDLER(LSU, handleMMULookupReq_));

            pipeline.registerHandlerAtStage(cache_lookup_stage_,
                                            CREATE_SPARTA_HANDLER(LSU, handleCacheLookupReq_));

            pipeline.registerHandlerAtStage(cache_read_stage_,
                                            CREATE_SPARTA_HANDLER(LSU, handleCacheRead_));

            pipeline.registerHandlerAtStage(complete_stage_,
                                            CREATE_SPARTA_HANDLER(LSU, completeInst_));
        }
        // // Allow the pipeline to create events and schedule work
        // ldst_pipeline_.performOwnUpdates();

        // // There can be situations where NOTHING is going on in the
        // // simulator but forward progression of the pipeline elements.
        // // In this case, the internal event for the LS pipeline will
        // // be the only event keeping simulation alive.  Sparta
        // // supports identifying non-essential events (by calling
        // // setContinuing to false on any event).
        // ldst_pipeline_.setContinuing(true);

        // ldst_pipeline_.registerHandlerAtStage(
        //     address_calculation_stage_, CREATE_SPARTA_HANDLER(LSU, handleAddressCalculation_));

        // ldst_pipeline_.registerHandlerAtStage(mmu_lookup_stage_,
        //                                       CREATE_SPARTA_HANDLER(LSU, handleMMULookupReq_));

        // ldst_pipeline_.registerHandlerAtStage(cache_lookup_stage_,
        //                                       CREATE_SPARTA_HANDLER(LSU, handleCacheLookupReq_));

        // ldst_pipeline_.registerHandlerAtStage(cache_read_stage_,
        //                                       CREATE_SPARTA_HANDLER(LSU, handleCacheRead_));

        // ldst_pipeline_.registerHandlerAtStage(complete_stage_,
        //                                       CREATE_SPARTA_HANDLER(LSU, completeInst_));

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
        if ((false == rob_stopped_simulation_) && (false == reservation_station_.empty()))
        {
            dumpDebugContent_(std::cerr);
            sparta_assert(false, "Reservation station has pending instructions");
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Callbacks
    ////////////////////////////////////////////////////////////////////////////////

    // Send initial credits (reservation_station_size_) to Dispatch Unit
    void LSU::sendInitialCredits_()
    {
        setupScoreboard_();
        out_lsu_credits_.send(reservation_station_size_);

        ILOG("LSU initial credits for Dispatch Unit: " << reservation_station_size_);
    }

    // Setup scoreboard View
    void LSU::setupScoreboard_()
    {
        // Setup scoreboard view upon register file
        // if we ever move to multicore, we only want to have resources look for scoreboard in their
        // cpu if we're running a test where we only have top.rename or top.issue_queue, then we can
        // just use the root
        auto cpu_node = getContainer()->findAncestorByName("core.*");
        if (cpu_node == nullptr)
        {
            cpu_node = getContainer()->getRoot();
        }
        for (uint32_t rf = 0; rf < core_types::RegFile::N_REGFILES;
            ++rf) // for (const auto rf : reg_files)
        {
            scoreboard_views_[rf].reset(new sparta::ScoreboardView(
                getContainer()->getName(), core_types::regfile_names[rf], cpu_node));
        }
    }

    // Receive new load/store instruction from Dispatch Unit
    // UPDATE:New method to handle instruction reception from dispatch
    void LSU::receiveInst_(const InstPtr & inst_ptr)
    {
        ILOG("New instruction received: " << inst_ptr);
        ReservationStationEntry entry{inst_ptr, false, 0, false, inst_ptr->isStoreInst()};
        if (reservation_station_.size() < reservation_station_size_)
        {
            reservation_station_.push_back(entry);
            handleOperandIssueCheck_(inst_ptr);
            insts_received_++;
        }
        else
        {
            // Handle full reservation station (e.g., stall or backpressure)
            ILOG("Reservation station full, cannot accept instruction: " << inst_ptr);
        }
    }

    // Callback from Scoreboard to inform Operand Readiness
    void LSU::handleOperandIssueCheck_(const InstPtr & inst_ptr)
    {
        if (inst_ptr->getStatus() == Inst::Status::SCHEDULED)
        {
            ILOG("Instruction was previously ready " << inst_ptr);
            return;
        }

        bool all_ready = instOperandReady_(inst_ptr);
       
        // Update reservation station entry
        for (auto& entry : reservation_station_)
        {
            if (entry.inst == inst_ptr)
            {
                entry.operands_ready = all_ready;
                break;
            }
        }

        if (all_ready)
        {
            updateIssuePriorityAfterNewDispatch_(inst_ptr);
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

        // Update reservation station entry if it exists
        for (auto& entry : reservation_station_)
        {
            if (entry.inst == inst_ptr)
            {
                entry.operands_ready = true;
                break;
            }
        }

        updateIssuePriorityAfterStoreInstRetire_(inst_ptr);
        if (isReadyToIssueInsts_())
        {
            ILOG("ROB Ack issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }

        ILOG("ROB Ack: Retired store instruction: " << inst_ptr);
    }

    // Issue/Re-issue ready instructions in the reservation station
    void LSU::issueInst_()
    {
        for (auto& pipeline : lsu_pipelines)
        {
            if (!pipeline.isBusy(address_calculation_stage_))
            {
                for (auto& entry : reservation_station_)
                {
                    if (entry.operands_ready && (entry.address_ready || !entry.is_store))
                    {
                        pipeline.append(entry.inst);
                        lsu_insts_issued_++;

                        // Remove from reservation station
                        reservation_station_.erase(std::remove_if(reservation_station_.begin(), reservation_station_.end(),
                            [&entry](const ReservationStationEntry& e) { return e.inst == entry.inst; }),
                            reservation_station_.end());
                       
                        // Handle speculative execution
                        if (allow_speculative_load_exec_ && !entry.is_store)
                        {
                            appendToReplayQueue_(createLoadStoreInst_(entry.inst));
                        }
                       
                        ILOG("Issued instruction: " << entry.inst);
                        break;
                    }
                }
            }
        }

        // Schedule another instruction issue event if possible
        if (isReadyToIssueInsts_())
        {
            ILOG("IssueInst_ issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(1));
        }
    }

    void LSU::handleAddressCalculation_()
    {
        for (auto& pipeline : lsu_pipelines)
        {
            if (pipeline.isValid(address_calculation_stage_))
            {
                auto & inst_ptr = pipeline[address_calculation_stage_];
                // Perform address calculation
                // Update reservation station entry
                for (auto& entry : reservation_station_)
                {
                    if (entry.inst == inst_ptr)
                    {
                        entry.address_ready = true;
                        entry.effective_address = calculateEffectiveAddress_(inst_ptr);
                        break;
                    }
                }
                ILOG("Address Generation " << inst_ptr);
            }
        }
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
        for (int i = 0; i < lsu_pipelines.size(); ++i)
        {
            auto& pipeline = lsu_pipelines[i];
            if (pipeline.isValid(mmu_lookup_stage_) && shared_resource_manager_.requestMMUAccess(i))
            {
                auto & inst_ptr = pipeline[mmu_lookup_stage_];
                MemoryAccessInfoPtr mem_access_info = createMemoryAccessInfo_(inst_ptr);
               
                const bool mmu_bypass =
                    (mem_access_info->getMMUState() == MemoryAccessInfo::MMUState::HIT);

                if (mmu_bypass)
                {
                    ILOG("MMU Lookup is skipped (TLB is already hit)! " << inst_ptr);
                    continue;
                }

                // Ready dependent younger loads
                if (false == allow_speculative_load_exec_)
                {
                    if (inst_ptr->isStoreInst())
                    {
                        readyDependentLoads_(createLoadStoreInst_(inst_ptr));
                    }
                }

                out_mmu_lookup_req_.send(mem_access_info);
                ILOG(mem_access_info << inst_ptr);
                break;
            }
        }
    }

    void LSU::getAckFromMMU_(const MemoryAccessInfoPtr & updated_memory_access_info_ptr)
    {
        ILOG("MMU Ack: " << std::boolalpha << updated_memory_access_info_ptr->getPhyAddrStatus()
                        << " " << updated_memory_access_info_ptr);
        const bool mmu_hit_ = updated_memory_access_info_ptr->getPhyAddrStatus();

        if (updated_memory_access_info_ptr->getInstPtr()->isStoreInst() && mmu_hit_
            && allow_speculative_load_exec_)
        {
            ILOG("Aborting speculative loads " << updated_memory_access_info_ptr);
            abortYoungerLoads_(updated_memory_access_info_ptr);
        }
        // Release the MMU
        shared_resource_manager_.releaseMMU();
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
        for (int i = 0; i < lsu_pipelines.size(); ++i)
        {  
            auto& pipeline = lsu_pipelines[i];
            if (pipeline.isValid(cache_lookup_stage_) && shared_resource_manager_.requestCacheAccess(i))
            {
                auto & inst_ptr = pipeline[cache_lookup_stage_];
                MemoryAccessInfoPtr mem_access_info = createMemoryAccessInfo_(inst_ptr);
                const bool phy_addr_is_ready = mem_access_info->getPhyAddrStatus();

                // If we did not have an MMU hit from previous stage, invalidate and bail
                if (false == phy_addr_is_ready)
                {
                    ILOG("Cache Lookup is skipped (Physical address not ready)!" << inst_ptr);
                    if (allow_speculative_load_exec_)
                    {
                        updateInstReplayReady_(createLoadStoreInst_(inst_ptr));
                    }
                    pipeline.invalidateStage(cache_lookup_stage_);
                    continue;
                }

                ILOG(inst_ptr << " " << mem_access_info);

                const bool is_already_hit =
                    (mem_access_info->getCacheState() == MemoryAccessInfo::CacheState::HIT);
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
                    continue;
                }

                out_cache_lookup_req_.send(mem_access_info);
                break;
            }
        }
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
        // Release the cache
        shared_resource_manager_.releaseCache();
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
        for (auto& pipeline : lsu_pipelines)
        {
            if (pipeline.isValid(cache_read_stage_))
            {
                auto & inst_ptr = pipeline[cache_read_stage_];
                MemoryAccessInfoPtr mem_access_info = createMemoryAccessInfo_(inst_ptr);
                ILOG(mem_access_info);

                if (false == mem_access_info->isCacheHit())
                {
                    ILOG("Cannot complete inst, cache miss: " << mem_access_info);
                    if (allow_speculative_load_exec_)
                    {
                        updateInstReplayReady_(createLoadStoreInst_(inst_ptr));
                    }
                    pipeline.invalidateStage(cache_read_stage_);
                    continue;
                }

                if (mem_access_info->isDataReady())
                {
                    ILOG("Instruction had previously had its data ready");
                    continue;
                }

                ILOG("Data ready set for " << mem_access_info);
                mem_access_info->setDataReady(true);
            }
        }


        if (isReadyToIssueInsts_())
        {
            ILOG("Cache read issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Retire load/store instruction
    void LSU::completeInst_()
    {
        for (auto& pipeline : lsu_pipelines)
        {
            if (pipeline.isValid(complete_stage_))
            {
                auto & inst_ptr = pipeline[complete_stage_];
                MemoryAccessInfoPtr mem_access_info = createMemoryAccessInfo_(inst_ptr);

                if (false == mem_access_info->isDataReady())
                {
                    ILOG("Cannot complete inst, cache data is missing: " << mem_access_info);
                    continue;
                }

                const bool is_store_inst = inst_ptr->isStoreInst();
                ILOG("Completing inst: " << inst_ptr);
                ILOG(mem_access_info);

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
                    sparta_assert(mem_access_info->getCacheState() == MemoryAccessInfo::CacheState::HIT,
                                "Load instruction cannot complete when cache is still a miss! "
                                    << mem_access_info);

                    // Mark instruction as completed
                    inst_ptr->setStatus(Inst::Status::COMPLETED);

                    if (allow_speculative_load_exec_)
                    {
                        ILOG("Removed replay " << inst_ptr);
                        removeInstFromReplayQueue_(inst_ptr);
                    }
    lsu_insts_completed_++;
                    out_lsu_credits_.send(1, 0);

                    ILOG("Complete Load Instruction: " << inst_ptr->getMnemonic() << " uid("
                                                    << inst_ptr->getUniqueID() << ")");
                }
                else // Complete store instruction
                {
                    if (inst_ptr->getStatus() != Inst::Status::RETIRED)
                    {
                        sparta_assert(mem_access_info->getMMUState() == MemoryAccessInfo::MMUState::HIT,
                                    "Store instruction cannot complete when TLB is still a miss!");

                        ILOG("Store was completed but waiting for retire " << inst_ptr);
                    }
                    else // Finish store operation
                    {
                        sparta_assert(mem_access_info->getCacheState() == MemoryAccessInfo::CacheState::HIT,
                                    "Store inst cannot finish when cache is still a miss! " << inst_ptr);

                        sparta_assert(mem_access_info->getMMUState() == MemoryAccessInfo::MMUState::HIT,
                                    "Store inst cannot finish when MMU is still a miss! " << inst_ptr);

                        if (allow_speculative_load_exec_)
                        {
                            ILOG("Removed replay " << inst_ptr);
                            removeInstFromReplayQueue_(inst_ptr);
                        }

                        lsu_insts_completed_++;
                        out_lsu_credits_.send(1, 0);

                        ILOG("Store operation is done!");
                    }
                }

                pipeline.invalidateStage(complete_stage_);
            }
        }

        if (isReadyToIssueInsts_())
        {
            ILOG("Complete issue");
            uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Handle instruction flush in LSU
    void LSU::handleFlush_(const FlushCriteria & criteria)
    {
        ILOG("Start Flushing!");

        lsu_flushes_++;

        // Flush reservation station
        auto rs_iter = reservation_station_.begin();
        while (rs_iter != reservation_station_.end())
        {
            if (criteria.includedInFlush(rs_iter->inst))
            {
                rs_iter = reservation_station_.erase(rs_iter);
            }
            else
            {
                ++rs_iter;
            }
        }

        // Flush load/store pipeline entry
        for (auto& pipeline : lsu_pipelines)
        {
            pipeline.flush([&](const InstPtr& inst) {
                return criteria.includedInFlush(inst);
            });
        }

        // Flush replay buffer and ready queue
        flushReplayBuffer_(criteria);
        flushReadyQueue_(criteria);

        // Cancel replay events
        auto flush = [&criteria](const LoadStoreInstInfoPtr & ldst_info_ptr) -> bool
        { return criteria.includedInFlush(ldst_info_ptr->getInstPtr()); };
        uev_append_ready_.cancelIf(flush);
        uev_replay_ready_.cancelIf(flush);

        // Cancel issue event already scheduled if no ready-to-issue inst left after flush
        if (!isReadyToIssueInsts_())
        {
            uev_issue_inst_.cancel();
        }
    }

    void LSU::replayReady_(const LoadStoreInstInfoPtr & replay_inst_ptr)
    {
        ILOG("Replay inst ready " << replay_inst_ptr);
        // We check in the reservation station as the instruction may not be in the replay queue
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
        ILOG("Appending to Ready queue " << replay_inst_ptr->isInReadyQueue() << " "
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


    void LSU::allocateInstToReservationStation_(const InstPtr & inst_ptr)
    {
        sparta_assert(reservation_station_.size() < reservation_station_size_,
                    "Appending reservation station causes overflows!");

        // Add new instruction to the reservation station
        ReservationStationEntry entry{inst_ptr, false, 0, false, inst_ptr->isStoreInst()};
        reservation_station_.push_back(entry);

        ILOG("Append new load/store instruction to reservation station!");
    }

    bool LSU::allOlderStoresIssued_(const InstPtr & inst_ptr)
    {
        for (const auto & entry : reservation_station_)
        {
            const auto & ldst_inst_ptr = entry.inst;
            if (ldst_inst_ptr->isStoreInst()
                && ldst_inst_ptr->getUniqueID() < inst_ptr->getUniqueID()
                && !entry.address_ready && ldst_inst_ptr != inst_ptr)
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
        for (auto & entry : reservation_station_)
        {
            auto & inst_ptr = entry.inst;
            if (inst_ptr->isStoreInst())
            {
                continue;
            }

            // Only ready loads which have register operands ready
            // We only care of the instructions which are still not ready
            // Instruction have a status of SCHEDULED if they are ready to be issued
            if (inst_ptr->getStatus() == Inst::Status::DISPATCHED && instOperandReady_(inst_ptr))
            {
                ILOG("Updating inst to schedule " << inst_ptr);
                updateIssuePriorityAfterNewDispatch_(inst_ptr);
                entry.operands_ready = true;
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

        for (auto& pipeline : lsu_pipelines)
        {
            for (int stage = 0; stage <= complete_stage_; stage++)
            {
                if (pipeline.isValid(stage))
                {
                    const auto & pipeline_inst = pipeline[stage];
                    if (pipeline_inst == load_store_inst_info_ptr->getInstPtr())
                    {
                        pipeline.invalidateStage(stage);
                        return;
                    }
                }
            }
        }
    }

    void LSU::removeInstFromReplayQueue_(const InstPtr & inst_to_remove)
    {
        ILOG("Removing Inst from replay queue " << inst_to_remove);
        auto iter = std::find_if(replay_buffer_.begin(), replay_buffer_.end(),
            [&inst_to_remove](const LoadStoreInstInfoPtr& info) {
                return info->getInstPtr() == inst_to_remove;
            });
       
        if (iter != replay_buffer_.end()) {
            replay_buffer_.erase(iter);
        }
    }

    void LSU::removeInstFromReplayQueue_(const LoadStoreInstInfoPtr & inst_to_remove)
    {
        ILOG("Removing Inst from replay queue " << inst_to_remove);
        auto iter = std::find(replay_buffer_.begin(), replay_buffer_.end(), inst_to_remove);
       
        if (iter != replay_buffer_.end()) {
            replay_buffer_.erase(iter);
        }
    }

    void LSU::appendToReplayQueue_(const LoadStoreInstInfoPtr & inst_info_ptr)
    {
        sparta_assert(replay_buffer_.size() < replay_buffer_size_,
                    "Appending replay queue causes overflows!");

        replay_buffer_.push_back(inst_info_ptr);

        ILOG("Append new instruction to replay queue!" << inst_info_ptr);
    }

    void LSU::appendToReadyQueue_(const InstPtr & inst_ptr)
    {
        for (const auto & entry : reservation_station_)
        {
            if (entry.inst == inst_ptr)
            {
                appendToReadyQueue_(createLoadStoreInst_(inst_ptr));
                return;
            }
        }

        sparta_assert(false, "Instruction not found in the reservation station " << inst_ptr);
    }

    void LSU::appendToReadyQueue_(const LoadStoreInstInfoPtr & ldst_inst_ptr)
    {
        ILOG("Appending to Ready queue " << ldst_inst_ptr);
        for (const auto & inst : ready_queue_)
        {
            sparta_assert(inst != ldst_inst_ptr, "Instruction already in ready queue " << ldst_inst_ptr);
        }
        ready_queue_.insert(ldst_inst_ptr);
        ldst_inst_ptr->setInReadyQueue(true);
        ldst_inst_ptr->setState(LoadStoreInstInfo::IssueState::READY);
    }

    // Arbitrate instruction issue from reservation station
    LSU::LoadStoreInstInfoPtr LSU::arbitrateInstIssue_()
    {
        sparta_assert(ready_queue_.size() > 0, "Arbitration fails: ready queue is empty!");

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
        for (auto & entry : reservation_station_)
        {
            if (entry.inst == inst_ptr)
            {
                auto inst_info_ptr = createLoadStoreInst_(inst_ptr);
                inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::NEW_DISP);

                // Update instruction status
                inst_ptr->setStatus(Inst::Status::SCHEDULED);
                return;
            }
        }

        sparta_assert(
            false, "Attempt to update issue priority for instruction not yet in the reservation station!");
    }

    // Update issue priority after tlb reload
    void LSU::updateIssuePriorityAfterTLBReload_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        bool is_found = false;
        for (auto & entry : reservation_station_)
        {
            if (entry.inst == inst_ptr)
            {
                auto inst_info_ptr = createLoadStoreInst_(inst_ptr);
                // Update issue priority for this outstanding TLB miss
                inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::MMU_RELOAD);
                uev_append_ready_.preparePayload(inst_info_ptr)->schedule(sparta::Clock::Cycle(0));

                is_found = true;
                break;
            }
        }

        sparta_assert(inst_ptr->getFlushedStatus() || is_found,
                    "Attempt to rehandle TLB lookup for instruction not yet in the reservation station! "
                        << inst_ptr);
    }

    // Update issue priority after cache reload
    void LSU::updateIssuePriorityAfterCacheReload_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();

        sparta_assert(inst_ptr->getFlushedStatus() == false,
                    "Attempt to rehandle cache lookup for flushed instruction!");

        bool is_found = false;
        for (auto & entry : reservation_station_)
        {
            if (entry.inst == inst_ptr)
            {
                auto inst_info_ptr = createLoadStoreInst_(inst_ptr);
                // Update issue priority for this outstanding cache miss
                inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_RELOAD);
                uev_append_ready_.preparePayload(inst_info_ptr)->schedule(sparta::Clock::Cycle(0));

                is_found = true;
                break;
            }
        }

        sparta_assert(is_found,
            "Attempt to rehandle cache lookup for instruction not yet in the reservation station! "
                << mem_access_info_ptr);
    }

    // Update issue priority after store instruction retires
    void LSU::updateIssuePriorityAfterStoreInstRetire_(const InstPtr & inst_ptr)
    {
        for (auto & entry : reservation_station_)
        {
            if (entry.inst == inst_ptr)
            {
                auto inst_info_ptr = createLoadStoreInst_(inst_ptr);
                inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_PENDING);
                uev_append_ready_.preparePayload(inst_info_ptr)->schedule(sparta::Clock::Cycle(0));
                return;
            }
        }

        sparta_assert(
            false, "Attempt to update issue priority for instruction not yet in the reservation station!");
    }

    bool LSU::olderStoresExists_(const InstPtr & inst_ptr)
    {
        for (const auto & entry : reservation_station_)
        {
            const auto & ldst_inst_ptr = entry.inst;
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

        auto iter = reservation_station_.begin();
        while (iter != reservation_station_.end())
        {
            auto inst_ptr = iter->inst;

            if (criteria.includedInFlush(inst_ptr))
            {
                iter = reservation_station_.erase(iter);

                // Clear any scoreboard callback
                std::vector<core_types::RegFile> reg_files = {core_types::RF_INTEGER,
                                                            core_types::RF_FLOAT};
                for (const auto rf : reg_files)
                {
                    scoreboard_views_[rf]->clearCallbacks(inst_ptr->getUniqueID());
                }

                ++credits_to_send;

                ILOG("Flush Instruction ID: " << inst_ptr->getUniqueID());
            }
            else
            {
                ++iter;
            }
        }

        if (credits_to_send > 0)
        {
            out_lsu_credits_.send(credits_to_send);

            ILOG("Flush " << credits_to_send << " instructions in reservation station!");
        }
    }

    // Flush load/store pipe
    void LSU::flushLSPipeline_(const FlushCriteria & criteria)
    {
        for (auto& pipeline : lsu_pipelines)
        {
            uint32_t stage_id = 0;
            for (auto iter = pipeline.begin(); iter != pipeline.end(); iter++, stage_id++)
            {
                // If the pipe stage is already invalid, no need to check criteria
                if (!iter.isValid())
                {
                    continue;
    }

                auto inst_ptr = *iter;
                if (criteria.includedInFlush(inst_ptr))
                {
                    pipeline.flushStage(iter);

                    ILOG("Flush Pipeline Stage[" << stage_id
                                                << "], Instruction ID: " << inst_ptr->getUniqueID());
                }
            }
        }
    }

    void LSU::flushReadyQueue_(const FlushCriteria & criteria)
    {
        auto iter = ready_queue_.begin();
        while (iter != ready_queue_.end())
        {
            auto inst_ptr = (*iter)->getInstPtr();

            auto delete_iter = iter++;

            if (criteria.includedInFlush(inst_ptr))
            {
                ready_queue_.erase(delete_iter);
                ILOG("Flushing from ready queue - Instruction ID: " << inst_ptr->getUniqueID());
            }
        }
    }

    void LSU::flushReplayBuffer_(const FlushCriteria & criteria)
    {
        auto iter = replay_buffer_.begin();
        while (iter != replay_buffer_.end())
        {
            auto inst_ptr = (*iter)->getInstPtr();

            auto delete_iter = iter++;

            if (criteria.includedInFlush(inst_ptr))
            {
                replay_buffer_.erase(delete_iter);
                ILOG("Flushing from replay buffer - Instruction ID: " << inst_ptr->getUniqueID());
            }
        }
    }

    MemoryAccessInfoPtr LSU::createMemoryAccessInfo_(const InstPtr & inst_ptr)
    {
        return sparta::allocate_sparta_shared_pointer<MemoryAccessInfo>(memory_access_allocator_, inst_ptr);
    }

    uint64_t LSU::calculateEffectiveAddress_(const InstPtr & inst_ptr)
    {
        // Implement effective address calculation
        // This is a placeholder implementation. Replace with actual logic.
        return inst_ptr->getTargetVAddr();
    }

    void LSU::updateReservationStation_()
    {
        bool ready_inst_exists = false;
        for (auto& entry : reservation_station_)
        {
            if (entry.operands_ready && (entry.address_ready || !entry.is_store))
            {
                ready_inst_exists = true;
                break;
            }
        }

        if (ready_inst_exists)
        {
            ev_issue_inst_.schedule();
        }
    }

    void LSU::dumpDebugContent_(std::ostream & output) const
    {
        output << "LSU Contents" << std::endl;
        output << "Reservation Station:" << std::endl;
        for (const auto & entry : reservation_station_)
        {
            output << '\t' << entry.inst << " Address Ready: " << entry.address_ready
                << " Operands Ready: " << entry.operands_ready << std::endl;
        }
        output << "Ready Queue:" << std::endl;
        for (const auto & entry : ready_queue_)
        {
            output << '\t' << entry->getInstPtr() << std::endl;
        }
        output << "Replay Buffer:" << std::endl;
        for (const auto & entry : replay_buffer_)
        {
            output << '\t' << entry->getInstPtr() << std::endl;
        }
        output << "Pipelines:" << std::endl;
        for (size_t i = 0; i < lsu_pipelines.size(); ++i)
        {
            output << "Pipeline " << i << ":" << std::endl;
            for (uint32_t stage = 0; stage <= complete_stage_; ++stage)
            {
                if (lsu_pipelines[i].isValid(stage))
                {
                    output << "\tStage " << stage << ": " << lsu_pipelines[i][stage] << std::endl;
                }
            }
        }
    }

    // Helper method to check if an instruction is in any of the LSU pipelines
    bool LSU::isInPipeline_(const InstPtr & inst_ptr) const
    {
        for (const auto& pipeline : lsu_pipelines)
        {
            for (uint32_t stage = 0; stage <= complete_stage_; ++stage)
            {
                if (pipeline.isValid(stage) && pipeline[stage] == inst_ptr)
                {
                    return true;
                }
            }
        }
        return false;
    }

    // Helper method to find an available pipeline
    sparta::Pipeline<InstPtr>* LSU::findAvailablePipeline_()
    {
        for (auto& pipeline : lsu_pipelines)
        {
            if (!pipeline.isBusy(address_calculation_stage_))
            {
                return &pipeline;
            }
        }
        return nullptr;
    }

    // Helper method to check if any pipeline is available
    bool LSU::isPipelineAvailable_() const
    {
        for (const auto& pipeline : lsu_pipelines)
        {
            if (!pipeline.isBusy(address_calculation_stage_))
            {
                return true;
            }
        }
        return false;
    }
}