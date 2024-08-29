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
    class LSU : public sparta::Unit
    {
      public:
        /*!
         * \class LSUParameterSet
         * \brief Parameters for LSU model
         */
        class LSUParameterSet : public sparta::ParameterSet
        {
          public:
            //! Constructor for LSUParameterSet
            LSUParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

            // Parameters for ldst_inst_queue
            PARAMETER(uint32_t, ldst_inst_queue_size, 8, "LSU ldst inst queue size")
            PARAMETER(uint32_t, replay_buffer_size, ldst_inst_queue_size, "Replay buffer size")
            PARAMETER(uint32_t, replay_issue_delay, 3, "Replay Issue delay")
            // LSU microarchitecture parameters
            PARAMETER(
                bool, allow_speculative_load_exec, true,
                "Allow loads to proceed speculatively before all older store addresses are known")
            // Pipeline length
            PARAMETER(uint32_t, mmu_lookup_stage_length, 1, "Length of the mmu lookup stage")
            PARAMETER(uint32_t, cache_lookup_stage_length, 1, "Length of the cache lookup stage")
            PARAMETER(uint32_t, cache_read_stage_length, 1, "Length of the cache read stage")
            // UPDATE: pipelines & reservation station
            PARAMETER(uint32_t, num_pipelines, 1, "Number of LSU pipelines")
            PARAMETER(uint32_t, reservation_station_size, 16, "Size of the reservation station")

        };

        /*!
         * \brief Constructor for LSU
         * \note  node parameter is the node that represent the LSU and
         *        p is the LSU parameter set
         */
        LSU(sparta::TreeNode* node, const LSUParameterSet* p);

        //! Destroy the LSU
        ~LSU();

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

        // sparta::DataInPort<InstQueue::value_type> in_lsu_insts_{&unit_port_set_, "in_lsu_insts", 1};
        // UPDATE: Added new input port for instruction dispatch
        sparta::DataInPort<InstPtr> in_inst_dispatch_{&unit_port_set_, "in_inst_dispatch", 1};

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
        sparta::DataOutPort<uint32_t> out_lsu_credits_{&unit_port_set_, "out_lsu_credits"};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_mmu_lookup_req_{&unit_port_set_,
                                                                     "out_mmu_lookup_req", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_cache_lookup_req_{&unit_port_set_,
                                                                       "out_cache_lookup_req", 0};
        // UPDATE: Added new output port for completed instructions
        sparta::DataOutPort<InstPtr> out_inst_complete_{&unit_port_set_, "out_inst_complete"};

        ////////////////////////////////////////////////////////////////////////////////
        // Internal States
        ////////////////////////////////////////////////////////////////////////////////
       
        // UPDATE: Added new members for multiple pipelines and reservation station
        const uint32_t num_pipelines_;
        const uint32_t reservation_station_size_;

        struct ReservationStationEntry
        {
            InstPtr inst;
            bool address_ready;
            uint64_t effective_address;
            bool operands_ready;
            bool is_store;
        };

        using ReservationStation = sparta::Buffer<ReservationStationEntry>;
        ReservationStation reservation_station_;
        std::vector<sparta::Pipeline<InstPtr>> lsu_pipelines;

        // Add share resource manager, prevent multiple units from accessing MMU and Cache
        class SharedResourceManager {
            public:
                bool requestMMUAccess(int pipelineId) {
                    if (mmu_busy_) return false;
                    mmu_busy_ = true;
                    current_mmu_user_ = pipelineId;
                    return true;
                }

                bool requestCacheAccess(int pipelineId) {
                    if (cache_busy_) return false;
                    cache_busy_ = true;
                    current_cache_user_ = pipelineId;
                    return true;
                }

                void releaseMMU() { mmu_busy_ = false; }
                void releaseCache() { cache_busy_ = false; }

                int getCurrentMMUUser() const { return current_mmu_user_; }
                int getCurrentCacheUser() const { return current_cache_user_; }

            private:
                bool mmu_busy_ = false;
                bool cache_busy_ = false;
                int current_mmu_user_ = -1;
                int current_cache_user_ = -1;
            };

        SharedResourceManager shared_resource_manager_;
        // Issue Queue
        // using LoadStoreIssueQueue = sparta::Buffer<LoadStoreInstInfoPtr>;
        // LoadStoreIssueQueue ldst_inst_queue_;
        // const uint32_t ldst_inst_queue_size_;

        sparta::Buffer<LoadStoreInstInfoPtr> replay_buffer_;
        const uint32_t replay_buffer_size_;
        const uint32_t replay_issue_delay_;

        sparta::PriorityQueue<LoadStoreInstInfoPtr> ready_queue_;
        // MMU unit
        bool mmu_busy_ = false;

        // L1 Data Cache
        bool cache_busy_ = false;

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
        // using LoadStorePipeline = sparta::Pipeline<LoadStoreInstInfoPtr>;
        // LoadStorePipeline ldst_pipeline_;

        // LSU Microarchitecture parameters
        const bool allow_speculative_load_exec_;

        // ROB stopped simulation early, transactions could still be inflight.
        bool rob_stopped_simulation_ = false;

        ////////////////////////////////////////////////////////////////////////////////
        // Event Handlers
        ////////////////////////////////////////////////////////////////////////////////

        // Event to issue instruction
        sparta::UniqueEvent<> uev_issue_inst_{&unit_event_set_, "issue_inst",
                                              CREATE_SPARTA_HANDLER(LSU, issueInst_)};

        sparta::PayloadEvent<LoadStoreInstInfoPtr> uev_replay_ready_{
            &unit_event_set_, "replay_ready",
            CREATE_SPARTA_HANDLER_WITH_DATA(LSU, replayReady_, LoadStoreInstInfoPtr)};

        sparta::PayloadEvent<LoadStoreInstInfoPtr> uev_append_ready_{
            &unit_event_set_, "append_ready",
            CREATE_SPARTA_HANDLER_WITH_DATA(LSU, appendReady_, LoadStoreInstInfoPtr)};

        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks
        ////////////////////////////////////////////////////////////////////////////////
        // Send initial credits (ldst_inst_queue_size_) to Dispatch Unit
        void sendInitialCredits_();

        // Setup Scoreboard Views
        void setupScoreboard_();

        // Receive new load/store Instruction from Dispatch Unit
        void getInstsFromDispatch_(const InstPtr &);

        // Callback from Scoreboard to inform Operand Readiness
        void handleOperandIssueCheck_(const InstPtr & inst_ptr);

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

        // Handle instruction flush in LSU
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
        void dumpDebugContent_(std::ostream & output) const override final;

        ////////////////////////////////////////////////////////////////////////////////
        // Regular Function/Subroutine Call
        ////////////////////////////////////////////////////////////////////////////////

        LoadStoreInstInfoPtr createLoadStoreInst_(const InstPtr & inst_ptr);

        // void allocateInstToIssueQueue_(const InstPtr & inst_ptr);
        // UPDATE
        void allocateInstToReservationStation_(const InstPtr & inst_ptr);

        bool isInPipeline_(const InstPtr& inst) const;

        sparta::Pipeline<InstPtr>* findAvailablePipeline_();

        bool isPipelineAvailable_() const;

        MemoryAccessInfoPtr createMemoryAccessInfo_(const InstPtr & inst_ptr);

        uint64_t calculateEffectiveAddress_(const InstPtr & inst_ptr);

        void updateReservationStation_();

        bool olderStoresExists_(const InstPtr & inst_ptr);

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

        void appendToReadyQueue_(const InstPtr &);

        // Pop completed load/store instruction out of issue queue
        void popIssueQueue_(const LoadStoreInstInfoPtr &);

        // Arbitrate instruction issue from ldst_inst_queue
        LoadStoreInstInfoPtr arbitrateInstIssue_();

        // Check for ready to issue instructions
        bool isReadyToIssueInsts_() const;

        // Update issue priority after dispatch
        void updateIssuePriorityAfterNewDispatch_(const InstPtr &);

        // Update issue priority after TLB reload
        void updateIssuePriorityAfterTLBReload_(const MemoryAccessInfoPtr &);

        // Update issue priority after cache reload
        void updateIssuePriorityAfterCacheReload_(const MemoryAccessInfoPtr &);

        // Update issue priority after store instruction retires
        void updateIssuePriorityAfterStoreInstRetire_(const InstPtr &);

        // Flush instruction issue queue
        void flushIssueQueue_(const FlushCriteria &);

        // Flush load/store pipeline
        void flushLSPipeline_(const FlushCriteria &);

        // Flush Ready Queue
        void flushReadyQueue_(const FlushCriteria &);

        // Flush Replay Buffer
        void flushReplayBuffer_(const FlushCriteria &);

        // Counters
        // sparta::Counter lsu_insts_dispatched_{getStatisticSet(), "lsu_insts_dispatched",
        //                                       "Number of LSU instructions dispatched",
        //                                       sparta::Counter::COUNT_NORMAL};
        sparta::Counter stores_retired_{getStatisticSet(), "stores_retired",
                                        "Number of stores retired", sparta::Counter::COUNT_NORMAL};
        // sparta::Counter lsu_insts_issued_{getStatisticSet(), "lsu_insts_issued",
        //                                   "Number of LSU instructions issued",
        //                                   sparta::Counter::COUNT_NORMAL};
        sparta::Counter replay_insts_{getStatisticSet(), "replay_insts_",
                                      "Number of Replay instructions issued",
                                      sparta::Counter::COUNT_NORMAL};
        // sparta::Counter lsu_insts_completed_{getStatisticSet(), "lsu_insts_completed",
        //                                      "Number of LSU instructions completed",
        //                                      sparta::Counter::COUNT_NORMAL};
        sparta::Counter lsu_flushes_{getStatisticSet(), "lsu_flushes",
                                     "Number of instruction flushes at LSU",
                                     sparta::Counter::COUNT_NORMAL};

        sparta::Counter biu_reqs_{getStatisticSet(), "biu_reqs", "Number of BIU reqs",
                                  sparta::Counter::COUNT_NORMAL};

        // UPDATE
        sparta::Counter insts_received_{getStatisticSet(), "insts_received",
                                        "Number of instructions received by LSU",
                                        sparta::Counter::COUNT_NORMAL};
        sparta::Counter insts_issued_{getStatisticSet(), "insts_issued",
                                      "Number of instructions issued to LSU pipelines",
                                      sparta::Counter::COUNT_NORMAL};
        sparta::Counter insts_completed_{getStatisticSet(), "insts_completed",
                                         "Number of instructions completed by LSU",
                                         sparta::Counter::COUNT_NORMAL};

        friend class LSUTester;
    };

    class LSUTester;
} // namespace olympia