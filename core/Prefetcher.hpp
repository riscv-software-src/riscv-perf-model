#pragma once
#include "PrefetchEngineIF.hpp"
#include "PrefetcherIF.hpp"
#include "FlushManager.hpp"
#include "sparta/resources/Queue.hpp"
#include <memory>

namespace olympia
{

    /**
     * @brief Prefetcher unit for Instruction and Data prefetching
     * 
     * This prefetcher operates on memory access addresses to predict
     * and prefetch future cache lines. It can be configured
     * with different prefetch engines (e.g., next-line, stride, etc.)
     */
    class Prefetcher : public sparta::Unit, public PrefetcherIF<PrefetchEngineIF<>>
    {
      public:
        //! \brief Parameters for Prefetcher model
        class PrefetcherParameterSet : public sparta::ParameterSet
        {
          public:
            PrefetcherParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

            PARAMETER(std::string, prefetcher_type, "next_line", 
                      "Prefetcher type: next_line, stride, etc.");
            PARAMETER(uint32_t, num_to_prefetch, 2,
                      "Number of cache lines to prefetch per request");
            PARAMETER(uint32_t, cacheline_size, 64, "Cache line size (in bytes)");
            PARAMETER(uint32_t, req_queue_size, 8, "Input queue size");
            PARAMETER(bool, enable_prefetcher, false, "Enable/disable prefetching");
        };

        /*!
         * \brief Constructor for Prefetcher
         *
         * \param node The node that represents the Prefetcher
         * \param p The Prefetcher's parameter set
         */
        Prefetcher(sparta::TreeNode* node, const PrefetcherParameterSet* p);

        /*!
         * \brief Process incoming request (Instruction or Data)
         *
         * \param access Incoming memory access
         */
        void processIncomingReq(const olympia::MemoryAccessInfoPtr & access) override;

        /*!
         * \brief Flush handling
         *
         * \param criteria Flushing Criteria
         */
        void handleFlush(const olympia::FlushManager::FlushingCriteria & criteria) override;

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static constexpr char name[] = "prefetcher";

      private:
        //! Prefetcher enabled flag
        const bool prefetcher_enabled_;

        //! Prefetcher Queue credits
        uint32_t prefetcher_credits_ = 0;

        //! Incoming request queue
        sparta::Queue<olympia::MemoryAccessInfoPtr> req_queue_;

        //! Event to generate prefetches
        sparta::UniqueEvent<> ev_gen_prefetch_;

        //! Event to handle incoming requests
        sparta::UniqueEvent<> ev_handle_incoming_req_;

        //! Incoming prefetcher queue credits (from consumer)
        std::unique_ptr<sparta::DataInPort<uint32_t>> prefetcher_queue_credits_in_;

        //! Credits out for req queue (to producer)
        std::unique_ptr<sparta::DataOutPort<uint32_t>> req_queue_credits_out_;

        //! Send the initial credits
        void sendInitialCredits_();

        //! Helper function to handle incoming requests
        void handleIncomingReq_();

        //! Callback function called by prefetcher_queue_credits_in_ when credits
        //  come back from consumer
        void receivePrefetchQueueCredits_(const uint32_t & credits);

        //! Helper function used to generate prefetches
        void generatePrefetch_();
    };

} // namespace olympia
