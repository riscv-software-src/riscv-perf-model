
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
#include "LSU.hpp"

namespace olympia
{
    class VLSU : public LSU
    {
      public:
        /*!
         * \class VLSUParameterSet
         * \brief Parameters for VLSU model
         */
        class VLSUParameterSet : public LSUParameterSet
        {
          public:
            //! Constructor for VLSUParameterSet
            VLSUParameterSet(sparta::TreeNode* n) : LSUParameterSet(n) {}

            PARAMETER(uint32_t, mem_req_buffer_size, 16, "VLSU memory request queue size")
            PARAMETER(uint32_t, data_width, 64, "Number of bits load/store per cycle")
        };

        /*!
         * \brief Constructor for VLSU
         * \note  node parameter is the node that represent the VLSU and
         *        p is the VLSU parameter set
         */
        VLSU(sparta::TreeNode* node, const VLSUParameterSet* p);

        //! Destroy the VLSU
        ~VLSU() {}

        //! name of this resource.
        static const char name[];

      private:
        // Memory Request Queue
        LoadStoreIssueQueue mem_req_buffer_;
        const uint32_t mem_req_buffer_size_;

        // Modeling construct for instructions that are ready for memory request generation
        sparta::PriorityQueue<LoadStoreInstInfoPtr> mem_req_ready_queue_;

        // Data width
        const uint32_t data_width_;

        ////////////////////////////////////////////////////////////////////////////////
        // Event Handlers
        ////////////////////////////////////////////////////////////////////////////////
        sparta::UniqueEvent<> uev_gen_mem_ops_{&unit_event_set_, "gen_mem_ops",
                                               CREATE_SPARTA_HANDLER(VLSU, genMemoryRequests_)};

        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks
        ////////////////////////////////////////////////////////////////////////////////
        // Generate memory requests for a vector load or store
        void genMemoryRequests_();

        // Callback from Scoreboard to inform Operand Readiness
        void handleOperandIssueCheck_(const LoadStoreInstInfoPtr &) override;

        // Retire load/store instruction
        void completeInst_() override;

        // Handle instruction flush in LSU
        void handleFlush_(const FlushCriteria &) override;

        // When simulation is ending (error or not), this function
        // will be called
        void onStartingTeardown_() override;

        // Typically called when the simulator is shutting down due to an exception
        // writes out text to aid debug
        void dumpDebugContent_(std::ostream & output) const override;

        ////////////////////////////////////////////////////////////////////////////////
        // Regular Function/Subroutine Call
        ////////////////////////////////////////////////////////////////////////////////
        // Remove completed memory request from the memory request buffer
        void removeFromMemoryRequestBuffer_(const LoadStoreInstInfoPtr &);

        bool allOlderStoresIssued_(const InstPtr &) override;

        ////////////////////////////////////////////////////////////////////////////////
        // Flush helper methods
        ////////////////////////////////////////////////////////////////////////////////
        // Flush memory request ready queue
        void flushMemoryRequestReadyQueue_(const FlushCriteria &);

        // Flush memory request buffer
        void flushMemoryRequestBuffer_(const FlushCriteria &);

        ////////////////////////////////////////////////////////////////////////////////
        // Counters
        ////////////////////////////////////////////////////////////////////////////////
        sparta::Counter memory_requests_generated_{getStatisticSet(), "memory_requests_generated",
                                       "Number of memory requests generated from vector loads and stores",
                                       sparta::Counter::COUNT_NORMAL};

        friend class VLSUTester;
    };

    class VLSUTester;
} // namespace olympia
