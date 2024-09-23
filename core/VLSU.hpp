
#pragma once

#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/collection/Collectable.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/resources/Pipeline.hpp"
#include "sparta/resources/Buffer.hpp"
#include "sparta/resources/PriorityQueue.hpp"
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/simulation/State.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "sparta/resources/Scoreboard.hpp"

#include "cache/TreePLRUReplacement.hpp"

#include "Inst.hpp"
#include "CoreTypes.hpp"
#include "FlushManager.hpp"
#include "CacheFuncModel.hpp"
#include "MemoryAccessInfo.hpp"
#include "LoadStoreInstInfo.hpp"
#include "MMU.hpp"
#include "DCache.hpp"

namespace olympia
{
    class VLSU : public sparta::Unit
    {
      public:
        /*!
         * \class VLSUParameterSet
         * \brief Parameters for VLSU model
         */
        class VLSUParameterSet : public sparta::ParameterSet
        {
          public:
            //! Constructor for VLSUParameterSet
            VLSUParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

            // Parameters for ldst_inst_queue
            PARAMETER(uint32_t, mem_request_queue_size, 8, "VLSU mem request queue size")
            PARAMETER(uint32_t, inst_queue_size, 8, "VLSU inst queue size")
            PARAMETER(uint32_t, replay_buffer_size, mem_request_queue_size, "Replay buffer size")
            PARAMETER(uint32_t, replay_issue_delay, 3, "Replay Issue delay")
            // VLSU microarchitecture parameters
            PARAMETER(
                bool, allow_speculative_load_exec, true,
                "Allow loads to proceed speculatively before all older store addresses are known")
            // Pipeline length
            PARAMETER(uint32_t, mmu_lookup_stage_length, 1, "Length of the mmu lookup stage")
            PARAMETER(uint32_t, cache_lookup_stage_length, 1, "Length of the cache lookup stage")
            PARAMETER(uint32_t, cache_read_stage_length, 1, "Length of the cache read stage")
            PARAMETER(uint32_t, data_width, 64, "Number of bits load/store per cycle")
        };

        /*!
         * \brief Constructor for VLSU
         * \note  node parameter is the node that represent the VLSU and
         *        p is the VLSU parameter set
         */
        VLSU(sparta::TreeNode* node, const VLSUParameterSet* p);

        //! Destroy the VLSU
        ~VLSU();

        //! name of this resource.
        static const char name[];

        ////////////////////////////////////////////////////////////////////////////////
        // Type Name/Alias Declaration
        ////////////////////////////////////////////////////////////////////////////////

        using LoadStoreInstInfoPtr = sparta::SpartaSharedPointer<LoadStoreInstInfo>;
        using LoadStoreInstIterator = sparta::Buffer<LoadStoreInstInfoPtr>::const_iterator;

        using FlushCriteria = FlushManager::FlushingCriteria;

      private:
        using ScoreboardViews =
            std::array<std::unique_ptr<sparta::ScoreboardView>, core_types::N_REGFILES>;

        ScoreboardViews scoreboard_views_;
        ////////////////////////////////////////////////////////////////////////////////
        // Input Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataInPort<InstQueue::value_type> in_vlsu_insts_{&unit_port_set_, "in_vlsu_insts",
                                                                 1};

        sparta::DataInPort<InstPtr> in_rob_retire_ack_{&unit_port_set_, "in_rob_retire_ack", 1};

        sparta::DataInPort<FlushCriteria> in_reorder_flush_{&unit_port_set_, "in_reorder_flush",
                                                            sparta::SchedulingPhase::Flush, 1};

        sparta::DataInPort<MemoryAccessInfoPtr> in_mmu_lookup_req_{&unit_port_set_,
                                                                   "in_mmu_lookup_req", 1};

        sparta::DataInPort<MemoryAccessInfoPtr> in_mmu_lookup_ack_{&unit_port_set_,
                                                                   "in_mmu_lookup_ack", 0};

        sparta::DataInPort<MemoryAccessInfoPtr> in_cache_lookup_req_{&unit_port_set_,
                                                                     "in_cache_lookup_req", 1};

        sparta::DataInPort<MemoryAccessInfoPtr> in_cache_lookup_ack_{&unit_port_set_,
                                                                     "in_cache_lookup_ack", 0};

        sparta::SignalInPort in_cache_free_req_{&unit_port_set_, "in_cache_free_req", 0};

        sparta::SignalInPort in_mmu_free_req_{&unit_port_set_, "in_mmu_free_req", 0};

        ////////////////////////////////////////////////////////////////////////////////
        // Output Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataOutPort<uint32_t> out_vlsu_credits_{&unit_port_set_, "out_vlsu_credits"};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_mmu_lookup_req_{&unit_port_set_,
                                                                     "out_mmu_lookup_req", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_cache_lookup_req_{&unit_port_set_,
                                                                       "out_cache_lookup_req", 0};

        ////////////////////////////////////////////////////////////////////////////////
        // Internal States
        ////////////////////////////////////////////////////////////////////////////////

        // Issue Queue
        using LoadStoreIssueQueue = sparta::Buffer<LoadStoreInstInfoPtr>;
        // holds loadstoreinfo memory requests
        LoadStoreIssueQueue mem_request_queue_;
        // holds inst_ptrs until done
        // one instruction can have multiple memory requests
        InstQueue inst_queue_;
        const uint32_t mem_request_queue_size_;
        const uint32_t inst_queue_size_;

        sparta::Buffer<LoadStoreInstInfoPtr> replay_buffer_;
        const uint32_t replay_buffer_size_;
        const uint32_t replay_issue_delay_;

        sparta::PriorityQueue<LoadStoreInstInfoPtr> ready_queue_;
        // MMU unit
        bool mmu_busy_ = false;

        // L1 Data Cache
        bool cache_busy_ = false;

        const uint32_t data_width_;

        sparta::collection::Collectable<bool> cache_busy_collectable_{getContainer(), "dcache_busy",
                                                                      &cache_busy_};

        // LSInstInfo allocator
        LoadStoreInstInfoAllocator & load_store_info_allocator_;

        // allocator for this object type
        MemoryAccessInfoAllocator & memory_access_allocator_;

        // NOTE:
        // Depending on which kind of cache (e.g. blocking vs. non-blocking) is being used
        // This single slot could potentially be extended to a cache pending miss queue

        const int address_calculation_stage_;
        const int mmu_lookup_stage_;
        const int cache_lookup_stage_;
        const int cache_read_stage_;
        const int complete_stage_;

        // Load/Store Pipeline
        using LoadStorePipeline = sparta::Pipeline<LoadStoreInstInfoPtr>;
        LoadStorePipeline ldst_pipeline_;

        // VLSU Microarchitecture parameters
        const bool allow_speculative_load_exec_;

        // ROB stopped simulation early, transactions could still be inflight.
        bool rob_stopped_simulation_ = false;

        ////////////////////////////////////////////////////////////////////////////////
        // Event Handlers
        ////////////////////////////////////////////////////////////////////////////////

        // Event to issue instruction
        sparta::UniqueEvent<> uev_issue_inst_{&unit_event_set_, "issue_inst",
                                              CREATE_SPARTA_HANDLER(VLSU, issueInst_)};

        sparta::UniqueEvent<> uev_gen_mem_ops_{&unit_event_set_, "gen_mem_ops",
                                               CREATE_SPARTA_HANDLER(VLSU, memRequestGenerator_)};

        sparta::PayloadEvent<LoadStoreInstInfoPtr> uev_replay_ready_{
            &unit_event_set_, "replay_ready",
            CREATE_SPARTA_HANDLER_WITH_DATA(VLSU, replayReady_, LoadStoreInstInfoPtr)};

        sparta::PayloadEvent<LoadStoreInstInfoPtr> uev_append_ready_{
            &unit_event_set_, "append_ready",
            CREATE_SPARTA_HANDLER_WITH_DATA(VLSU, appendReady_, LoadStoreInstInfoPtr)};

        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks
        ////////////////////////////////////////////////////////////////////////////////
        // Send initial credits (mem_request_queue_size_) to Dispatch Unit
        void sendInitialCredits_();

        // Setup Scoreboard Views
        void setupScoreboard_();

        // Receive new load/store Instruction from Dispatch Unit
        void getInstsFromDispatch_(const InstPtr &);

        // Callback from Scoreboard to inform Operand Readiness
        void handleOperandIssueCheck_(const LoadStoreInstInfoPtr & inst_ptr);

        // Receive update from ROB whenever store instructions retire
        void getAckFromROB_(const InstPtr &);

        // Issue/Re-issue ready instructions in the issue queue
        void issueInst_();

        // Calculate memory load/store address
        void handleAddressCalculation_();
        // Handle MMU access request
        void handleMMULookupReq_();
        void handleMMUReadyReq_(const MemoryAccessInfoPtr & memory_access_info_ptr);
        void getAckFromMMU_(const MemoryAccessInfoPtr & updated_memory_access_info_ptr);

        // Handle cache access request
        void handleCacheLookupReq_();
        void handleCacheReadyReq_(const MemoryAccessInfoPtr & memory_access_info_ptr);
        void getAckFromCache_(const MemoryAccessInfoPtr & updated_memory_access_info_ptr);

        // Perform cache read
        void handleCacheRead_();

        // Retire load/store instruction
        void completeInst_();

        // Handle instruction flush in VLSU
        void handleFlush_(const FlushCriteria &);

        // Instructions in the replay ready to issue
        void replayReady_(const LoadStoreInstInfoPtr &);

        // Mark instruction as not ready and schedule replay ready
        void updateInstReplayReady_(const LoadStoreInstInfoPtr &);

        // Instructions in the replay ready to issue
        void appendReady_(const LoadStoreInstInfoPtr &);

        // Called when ROB terminates the simulation
        void onROBTerminate_(const bool & val);

        // When simulation is ending (error or not), this function
        // will be called
        void onStartingTeardown_() override;

        // Typically called when the simulator is shutting down due to an exception
        // writes out text to aid debug
        // set as protected because VLSU dervies from LSU
        void dumpDebugContent_(std::ostream & output) const override final;

        ////////////////////////////////////////////////////////////////////////////////
        // Regular Function/Subroutine Call
        ////////////////////////////////////////////////////////////////////////////////

        LoadStoreInstInfoPtr createLoadStoreInst_(const InstPtr & inst_ptr);

        void memRequestGenerator_();

        void allocateInstToIssueQueue_(const InstPtr & inst_ptr);

        bool allOlderStoresIssued_(const InstPtr & inst_ptr);

        void readyDependentLoads_(const LoadStoreInstInfoPtr &);

        bool instOperandReady_(const InstPtr &);

        void abortYoungerLoads_(const olympia::MemoryAccessInfoPtr & memory_access_info_ptr);

        // Remove instruction from pipeline which share the same address
        void dropInstFromPipeline_(const LoadStoreInstInfoPtr &);

        // Append new store instruction into replay queue
        void appendToReplayQueue_(const LoadStoreInstInfoPtr & inst_info_ptr);

        // Pop completed load/store instruction out of replay queue
        void removeInstFromReplayQueue_(const LoadStoreInstInfoPtr & inst_to_remove);
        void removeInstFromReplayQueue_(const InstPtr & inst_to_remove);

        void appendToReadyQueue_(const LoadStoreInstInfoPtr &);

        // Pop completed load/store instruction out of issue queue
        void popIssueQueue_(const LoadStoreInstInfoPtr &);

        // Arbitrate instruction issue from ldst_inst_queue
        LoadStoreInstInfoPtr arbitrateInstIssue_();

        // Check for ready to issue instructions
        bool isReadyToIssueInsts_() const;

        // Update issue priority after dispatch
        void updateIssuePriorityAfterNewDispatch_(const LoadStoreInstInfoPtr &);

        // Update issue priority after TLB reload
        void updateIssuePriorityAfterTLBReload_(const MemoryAccessInfoPtr &);

        // Update issue priority after cache reload
        void updateIssuePriorityAfterCacheReload_(const MemoryAccessInfoPtr &);

        // Update issue priority after store instruction retires
        void updateIssuePriorityAfterStoreInstRetire_(const LoadStoreInstInfoPtr &);

        // Flush instruction issue queue
        void flushIssueQueue_(const FlushCriteria &);

        // Flush load/store pipeline
        void flushLSPipeline_(const FlushCriteria & criteria)
        {
            uint32_t stage_id = 0;
            for (auto iter = ldst_pipeline_.begin(); iter != ldst_pipeline_.end(); iter++, stage_id++)
            {
                // If the pipe stage is already invalid, no need to criteria
                if (!iter.isValid())
                {
                    continue;
                }

                auto inst_ptr = (*iter)->getInstPtr();
                if (criteria.includedInFlush(inst_ptr))
                {
                    ldst_pipeline_.flushStage(iter);
                    DLOG("Flush Pipeline Stage[" << stage_id
                                                 << "], Instruction ID: " << inst_ptr->getUniqueID());
                }
            }
        }

        // Flush Ready Queue
        void flushReadyQueue_(const FlushCriteria & criteria)
        {
            // TODO: Replace with erase_if with c++20
            auto iter = ready_queue_.begin();
            while (iter != ready_queue_.end())
            {
                auto inst_ptr = (*iter)->getInstPtr();
                if (criteria.includedInFlush(inst_ptr))
                {
                    ready_queue_.erase(++iter);
                    DLOG("Flushing from ready queue - Instruction ID: " << inst_ptr->getUniqueID());
                }
            }
        }

        // Flush Replay Buffer
        void flushReplayBuffer_(const FlushCriteria & criteria)
        {
            // TODO: Replace with erase_if with c++20
            auto iter = replay_buffer_.begin();
            while (iter != replay_buffer_.end())
            {
                auto inst_ptr = (*iter)->getInstPtr();
                if (criteria.includedInFlush(inst_ptr))
                {
                    replay_buffer_.erase(++iter);
                    DLOG("Flushing from replay buffer - Instruction ID: " << inst_ptr->getUniqueID());
                }
            }
        }

        // Counters
        sparta::Counter vlsu_insts_dispatched_{getStatisticSet(), "vlsu_insts_dispatched",
                                               "Number of VLSU instructions dispatched",
                                               sparta::Counter::COUNT_NORMAL};
        sparta::Counter vlsu_insts_issued_{getStatisticSet(), "vlsu_insts_issued",
                                           "Number of VLSU instructions issued",
                                           sparta::Counter::COUNT_NORMAL};
        sparta::Counter vlsu_mem_reqs_{getStatisticSet(), "vlsu_mem_reqs",
                                       "Number of memory requests allocated",
                                       sparta::Counter::COUNT_NORMAL};
        sparta::Counter vlsu_insts_replayed_{getStatisticSet(), "vlsu_insts_replayed",
                                             "Number of VLSU instructions replayed",
                                             sparta::Counter::COUNT_NORMAL};
        sparta::Counter vlsu_insts_completed_{getStatisticSet(), "vlsu_insts_completed",
                                              "Number of VLSU instructions completed",
                                              sparta::Counter::COUNT_NORMAL};
        sparta::Counter vlsu_stores_retired_{getStatisticSet(), "vlsu_stores_retired",
                                             "Number of stores retired in the VLSU",
                                             sparta::Counter::COUNT_NORMAL};
        sparta::Counter vlsu_flushes_{getStatisticSet(), "vlsu_flushes",
                                      "Number of flushes in the VLSU",
                                      sparta::Counter::COUNT_NORMAL};
        sparta::Counter vlsu_biu_reqs_{getStatisticSet(), "vlsu_biu_reqs", "Number of BIU requests from the VLSU",
                                       sparta::Counter::COUNT_NORMAL};

        friend class VLSUTester;
    };

    class VLSUTester;
} // namespace olympia
