
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
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/simulation/State.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "sparta/resources/Scoreboard.hpp"

#include "cache/TreePLRUReplacement.hpp"

#include "Inst.hpp"
#include "CoreTypes.hpp"
#include "FlushManager.hpp"
#include "SimpleDL1.hpp"
#include "MemoryAccessInfo.hpp"
#include "MMU.hpp"
#include "DCache.hpp"
#include <sparta/events/SchedulingPhases.hpp>

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
            LSUParameterSet(sparta::TreeNode* n):
                sparta::ParameterSet(n)
            {
            }

            // Parameters for ldst_inst_queue
            PARAMETER(uint32_t, ldst_inst_queue_size, 8, "LSU ldst inst queue size")
            PARAMETER(uint32_t, replay_buffer_size, ldst_inst_queue_size, "Replay buffer size")
            PARAMETER(uint32_t, replay_issue_delay, 3, "Replay Issue delay")
            // LSU microarchitecture parameters
            PARAMETER(bool, allow_speculative_load_exec, true, "Allow loads to proceed speculatively before all older store addresses are known")
            // Pipeline length
            PARAMETER(uint32_t, mmu_lookup_stage_length, 1, "Length of the mmu lookup stage")
            PARAMETER(uint32_t, cache_lookup_stage_length, 1, "Length of the cache lookup stage")
            PARAMETER(uint32_t, cache_read_stage_length, 1, "Length of the cache read stage")
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


        class LoadStoreInstInfo;

        using LoadStoreInstInfoPtr = sparta::SpartaSharedPointer<LoadStoreInstInfo>;
        using FlushCriteria = FlushManager::FlushingCriteria;

        using LoadStoreInstIterator = sparta::Buffer<LoadStoreInstInfoPtr>::const_iterator;
        // Forward declaration of the Pair Definition class is must as we are friending it.
        class LoadStoreInstInfoPairDef;
        // Keep record of instruction issue information
        class LoadStoreInstInfo
        {
        public:
            // The modeler needs to alias a type called "SpartaPairDefinitionType" to the Pair Definition class  of itself
            using SpartaPairDefinitionType = LoadStoreInstInfoPairDef;
            enum class IssuePriority : std::uint16_t
            {
                HIGHEST = 0,
                __FIRST = HIGHEST,
                CACHE_RELOAD,   // Receive mss ack, waiting for cache re-access
                CACHE_PENDING,  // Wait for another outstanding miss finish
                MMU_RELOAD,     // Receive for mss ack, waiting for mmu re-access
                MMU_PENDING,    // Wait for another outstanding miss finish
                NEW_DISP,       // Wait for new issue
                LOWEST,
                NUM_OF_PRIORITIES,
                __LAST = NUM_OF_PRIORITIES
            };

            enum class IssueState : std::uint32_t
            {
                READY = 0,          // Ready to be issued
                __FIRST = READY,
                ISSUED,         // On the flight somewhere inside Load/Store Pipe
                NOT_READY,      // Not ready to be issued
                NUM_STATES,
                __LAST = NUM_STATES
            };

            LoadStoreInstInfo() = delete;
            LoadStoreInstInfo(const MemoryAccessInfoPtr & info_ptr) :
                mem_access_info_ptr_(info_ptr),
                rank_(IssuePriority::LOWEST),
                state_(IssueState::NOT_READY){}

            // This Inst pointer will act as one of the two portals to the Inst class
            // and we will use this pointer to query values from functions of Inst class
            const InstPtr & getInstPtr() const {
                return mem_access_info_ptr_->getInstPtr();
            }

            // This MemoryAccessInfo pointer will act as one of the two portals to the MemoryAccesInfo class
            // and we will use this pointer to query values from functions of MemoryAccessInfo class
            const MemoryAccessInfoPtr & getMemoryAccessInfoPtr() const {
                return mem_access_info_ptr_;
            }

            // This is a function which will be added in the SPARTA_ADDPAIRs API.
            uint64_t getInstUniqueID() const {
                const MemoryAccessInfoPtr &mem_access_info_ptr = getMemoryAccessInfoPtr();
                return mem_access_info_ptr == nullptr ? 0 : mem_access_info_ptr->getInstUniqueID();
            }

            void setPriority(const IssuePriority & rank) {
                rank_.setValue(rank);
            }

            const IssuePriority & getPriority() const {
                return rank_.getEnumValue();
            }

            void setState(const IssueState & state) {
                state_.setValue(state);
             }

            const IssueState & getState() const {
                return state_.getEnumValue();
            }


            bool isReady() const { return (getState() == IssueState::READY); }

            bool isRetired() const { return getInstPtr()->getStatus() == Inst::Status::RETIRED; }

            bool winArb(const LoadStoreInstInfoPtr & that) const
            {
                if (that == nullptr) {
                    return true;
                }

                return (static_cast<uint32_t>(this->getPriority())
                    < static_cast<uint32_t>(that->getPriority()));
            }

            const LoadStoreInstIterator getIssueQueueIterator() const
            {
                return issue_queue_iterator_;
            }

            void setIssueQueueIterator(const LoadStoreInstIterator &iter){
                issue_queue_iterator_ = iter;
            }

            const LoadStoreInstIterator & getReplayQueueIterator() const
            {
                return replay_queue_iterator_;
            }

            void setReplayQueueIterator(const LoadStoreInstIterator & iter)
            {
                replay_queue_iterator_ = iter;
            }

          private:
            MemoryAccessInfoPtr mem_access_info_ptr_;
            sparta::State<IssuePriority> rank_;
            sparta::State<IssueState> state_;
            LoadStoreInstIterator issue_queue_iterator_;
            LoadStoreInstIterator replay_queue_iterator_;
        };  // class LoadStoreInstInfo

        using LoadStoreInstInfoAllocator = sparta::SpartaSharedPointerAllocator<LoadStoreInstInfo>;

        /*!
        * \class LoadStoreInstInfoPairDef
        * \brief Pair Definition class of the load store instruction that flows through the example/CoreModel
        */
        // This is the definition of the PairDefinition class of LoadStoreInstInfo.
        // This PairDefinition class could be named anything but it needs to inherit
        // publicly from sparta::PairDefinition templatized on the actual class LoadStoreInstInfo.
        class LoadStoreInstInfoPairDef : public sparta::PairDefinition<LoadStoreInstInfo>{
        public:

            // The SPARTA_ADDPAIRs APIs must be called during the construction of the PairDefinition class
            LoadStoreInstInfoPairDef() : PairDefinition<LoadStoreInstInfo>(){
                SPARTA_INVOKE_PAIRS(LoadStoreInstInfo);
            }
            SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("DID",   &LoadStoreInstInfo::getInstUniqueID),
                                  SPARTA_ADDPAIR("rank",  &LoadStoreInstInfo::getPriority),
                                  SPARTA_ADDPAIR("state", &LoadStoreInstInfo::getState),
                                  SPARTA_FLATTEN(         &LoadStoreInstInfo::getMemoryAccessInfoPtr))
        };
    private:

        using ScoreboardViews = std::array<std::unique_ptr<sparta::ScoreboardView>, core_types::N_REGFILES>;
        ScoreboardViews scoreboard_views_;
        ////////////////////////////////////////////////////////////////////////////////
        // Input Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataInPort<InstQueue::value_type> in_lsu_insts_
            {&unit_port_set_, "in_lsu_insts", 1};

        sparta::DataInPort<InstPtr> in_rob_retire_ack_
            {&unit_port_set_, "in_rob_retire_ack", 1};

        sparta::DataInPort<FlushCriteria> in_reorder_flush_
            {&unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};


        sparta::DataInPort<MemoryAccessInfoPtr> in_mmu_lookup_req_
                {&unit_port_set_, "in_mmu_lookup_req", 1};

        sparta::DataInPort<MemoryAccessInfoPtr> in_mmu_lookup_ack_
                {&unit_port_set_, "in_mmu_lookup_ack", 0};

        sparta::DataInPort<MemoryAccessInfoPtr> in_cache_data_ready_
                {&unit_port_set_, "in_cache_data_ready", 0};

        sparta::DataInPort<MemoryAccessInfoPtr> in_cache_lookup_resp_
                {&unit_port_set_, "in_cache_lookup_resp", sparta::SchedulingPhase::Update, 0};

        sparta::SignalInPort in_cache_lookup_nack_
                {&unit_port_set_, "in_cache_lookup_nack", 0};

        sparta::SignalInPort in_mmu_free_req_
                {&unit_port_set_, "in_mmu_free_req", 0};

        ////////////////////////////////////////////////////////////////////////////////
        // Output Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataOutPort<uint32_t> out_lsu_credits_
            {&unit_port_set_, "out_lsu_credits"};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_mmu_lookup_req_
            {&unit_port_set_, "out_mmu_lookup_req", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_cache_lookup_req_
                {&unit_port_set_, "out_cache_lookup_req", 0};

        ////////////////////////////////////////////////////////////////////////////////
        // Internal States
        ////////////////////////////////////////////////////////////////////////////////

        // Issue Queue
        using LoadStoreIssueQueue = sparta::Buffer<LoadStoreInstInfoPtr>;
        LoadStoreIssueQueue ldst_inst_queue_;
        const uint32_t ldst_inst_queue_size_;

        sparta::Buffer<LoadStoreInstInfoPtr> replay_buffer_;
        const uint32_t replay_buffer_size_;

        const uint32_t replay_issue_delay_;
        // MMU unit
        bool mmu_busy_ = false;
        bool mmu_pending_inst_flushed = false;


        // L1 Data Cache
        bool cache_busy_ = false;
        bool cache_pending_inst_flushed_ = false;

        sparta::collection::Collectable<bool> cache_busy_collectable_{
            getContainer(), "dcache_busy", &cache_busy_};

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

        // LSU Microarchitecture parameters
        const bool allow_speculative_load_exec_;

        ////////////////////////////////////////////////////////////////////////////////
        // Event Handlers
        ////////////////////////////////////////////////////////////////////////////////

        // Event to issue instruction
        sparta::UniqueEvent<> uev_issue_inst_{&unit_event_set_, "issue_inst",
                CREATE_SPARTA_HANDLER(LSU, issueInst_)};

        sparta::PayloadEvent<LoadStoreInstInfoPtr> uev_replay_ready_{&unit_event_set_, "replay_ready",
                CREATE_SPARTA_HANDLER_WITH_DATA(LSU, replayReady_, LoadStoreInstInfoPtr)};

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
        void handleMMUReadyReq_(const MemoryAccessInfoPtr &memory_access_info_ptr);
        void getAckFromMMU_(const MemoryAccessInfoPtr &updated_memory_access_info_ptr);

        // Handle cache access request
        void handleCacheLookupReq_();
        void handleCacheReadyReq_(const MemoryAccessInfoPtr &memory_access_info_ptr);
        void getAckFromCache_(const MemoryAccessInfoPtr &updated_memory_access_info_ptr);

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

        ////////////////////////////////////////////////////////////////////////////////
        // Regular Function/Subroutine Call
        ////////////////////////////////////////////////////////////////////////////////

        LoadStoreInstInfoPtr createLoadStoreInst_(const InstPtr &inst_ptr);

        void allocateInstToIssueQueue_(const InstPtr &inst_ptr);

        bool allOlderStoresIssued_(const InstPtr &inst_ptr);

        void readyDependentLoads_(const LoadStoreInstInfoPtr &);

        void abortYoungerLoads_(const olympia::MemoryAccessInfoPtr & memory_access_info_ptr);

        // Remove instruction from pipeline which share the same address
        void dropInstFromPipeline_(const LoadStoreInstInfoPtr &);

        // Append new store instruction into replay queue
        void appendToReplayQueue_(const LoadStoreInstInfoPtr & inst_info_ptr);

        // Pop completed load/store instruction out of replay queue
        void removeInstFromReplayQueue_(const LoadStoreInstInfoPtr & inst_to_remove);

        // Pop completed load/store instruction out of issue queue
        void popIssueQueue_(const LoadStoreInstInfoPtr &);

        // Arbitrate instruction issue from ldst_inst_queue
        const LoadStoreInstInfoPtr & arbitrateInstIssue_();

        // Check for ready to issue instructions
        bool isReadyToIssueInsts_() const;

        // Update issue priority after dispatch
        void updateIssuePriorityAfterNewDispatch_(const InstPtr &);

        // Update issue priority after TLB reload
        void updateIssuePriorityAfterTLBReload_(const InstPtr &,
                                                const bool = false);

        // Update issue priority after cache reload
        void updateIssuePriorityAfterCacheReload_(const InstPtr &,
                                                  const bool = false);

        // Update issue priority after store instruction retires
        void updateIssuePriorityAfterStoreInstRetire_(const InstPtr &);

            // Flush instruction issue queue
        template<typename Comp>
        void flushIssueQueue_(const Comp &);

        // Flush load/store pipeline
        template<typename Comp>
        void flushLSPipeline_(const Comp &);

        // Counters
        sparta::Counter lsu_insts_dispatched_{
            getStatisticSet(), "lsu_insts_dispatched",
            "Number of LSU instructions dispatched", sparta::Counter::COUNT_NORMAL
        };
        sparta::Counter stores_retired_{
            getStatisticSet(), "stores_retired",
            "Number of stores retired", sparta::Counter::COUNT_NORMAL
        };
        sparta::Counter lsu_insts_issued_{
            getStatisticSet(), "lsu_insts_issued",
            "Number of LSU instructions issued", sparta::Counter::COUNT_NORMAL
        };
        sparta::Counter replay_insts_{
            getStatisticSet(), "replay_insts_",
            "Number of Replay instructions issued", sparta::Counter::COUNT_NORMAL
        };
        sparta::Counter lsu_insts_completed_{
            getStatisticSet(), "lsu_insts_completed",
            "Number of LSU instructions completed", sparta::Counter::COUNT_NORMAL
        };
        sparta::Counter lsu_flushes_{
            getStatisticSet(), "lsu_flushes",
            "Number of instruction flushes at LSU", sparta::Counter::COUNT_NORMAL
        };

        sparta::Counter biu_reqs_{
            getStatisticSet(), "biu_reqs",
            "Number of BIU reqs", sparta::Counter::COUNT_NORMAL
        };


        // When simulation is ending (error or not), this function
        // will be called
        void onStartingTeardown_() override {}

        bool olderStoresExists_(const InstPtr & inst_ptr);

        friend class LSUTester;

    };


    inline std::ostream& operator<<(std::ostream& os,
        const olympia::LSU::LoadStoreInstInfo::IssuePriority& rank){
        switch(rank){
            case LSU::LoadStoreInstInfo::IssuePriority::HIGHEST:
                os << "(highest)";
                break;
            case LSU::LoadStoreInstInfo::IssuePriority::CACHE_RELOAD:
                os << "($_reload)";
                break;
            case LSU::LoadStoreInstInfo::IssuePriority::CACHE_PENDING:
                os << "($_pending)";
                break;
            case LSU::LoadStoreInstInfo::IssuePriority::MMU_RELOAD:
                os << "(mmu_reload)";
                break;
            case LSU::LoadStoreInstInfo::IssuePriority::MMU_PENDING:
                os << "(mmu_pending)";
                break;
            case LSU::LoadStoreInstInfo::IssuePriority::NEW_DISP:
                os << "(new_disp)";
                break;
            case LSU::LoadStoreInstInfo::IssuePriority::LOWEST:
                os << "(lowest)";
                break;
            case LSU::LoadStoreInstInfo::IssuePriority::NUM_OF_PRIORITIES:
                throw sparta::SpartaException("NUM_OF_PRIORITIES cannot be a valid enum state.");
        }
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os,
        const olympia::LSU::LoadStoreInstInfo::IssueState& state){
        // Print instruction issue state
        switch(state){
            case LSU::LoadStoreInstInfo::IssueState::READY:
                os << "(ready)";
                break;
            case LSU::LoadStoreInstInfo::IssueState::ISSUED:
                os << "(issued)";
                break;
            case LSU::LoadStoreInstInfo::IssueState::NOT_READY:
                os << "(not_ready)";
                break;
            case LSU::LoadStoreInstInfo::IssueState::NUM_STATES:
                throw sparta::SpartaException("NUM_STATES cannot be a valid enum state.");
        }
        return os;
    }


    inline std::ostream & operator<<(std::ostream & os,
                                     const olympia::LSU::LoadStoreInstInfo & ls_info)
    {
        os << "lsinfo: "
           << "uid: "    << ls_info.getInstUniqueID()
           << " pri:"    << ls_info.getPriority()
           << " state: " << ls_info.getState();
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os,
                                     const olympia::LSU::LoadStoreInstInfoPtr & ls_info)
    {
        os << *ls_info;
        return os;
    }

    class LSUTester;
} // namespace olympia
