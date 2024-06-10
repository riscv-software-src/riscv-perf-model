#pragma once
#include "PrefetchEngineIF.hpp"
#include "PrefetcherIF.hpp"
#include "sparta/resources/Queue.hpp"

namespace olympia {

  class DataPrefetcher  :  public sparta::Unit, public PrefetcherIF<PrefetchEngineIF<>> {
  public:
    //! \brief Parameters for DataPrefetcher model
    class DataPrefetcherParameterSet : public sparta::ParameterSet
    {
    public:

      DataPrefetcherParameterSet(sparta::TreeNode* n) :
        sparta::ParameterSet(n)
      { }
      PARAMETER(std::string, prefetcher_type,         "next_line", "next_line");
      PARAMETER(uint32_t   , num_to_prefetch,                   1, "Number of cache lines to prefetch per incoming memory access");
      PARAMETER(uint32_t   , cacheline_size,                    64, "cache line size (in bytes)");
      PARAMETER(uint32_t   , req_queue_size,                    8, "Input queue size");
     };

    /*!
     * \brief Constructor for DataPrefetcher
     *
     * \param node The node that represents the DataPrefetcher
     * \param p The DataPrefetcher's parameter set
     */
    DataPrefetcher(sparta::TreeNode *node,
                   const DataPrefetcherParameterSet * p);

    /*!
     * \brief Process of incoming request
     *
     * \param access Incoming memory accesses
     */
    void processIncomingReq(const olympia::MemoryAccessInfoPtr& access) override;


    /*!
     * \brief Flush handling
     *
     * \param criteria Flushing Criteria
     */
    void handleFlush(const olympia::FlushManager::FlushingCriteria & criteria) override;


    //! \brief Name of this resource. Required by sparta::UnitFactory
    static constexpr char name[] = "data_prefetcher";

  private:
    //! Prefetcher Queue credits
    uint32_t prefetcher_credits_ = 0;

    //! Incoming request queue
    sparta::Queue<olympia::MemoryAccessInfoPtr> req_queue_;

    //! Event to generate prefetches
    sparta::UniqueEvent<> ev_gen_prefetch_;

    //! Event to handle incoming requests
    sparta::UniqueEvent<> ev_handle_incoming_req_;

    //! Incoming prefetcher queue credits (from consumer of prefetch requests)
    sparta::DataInPort<uint32_t> prefetcher_queue_credits_in_;

    //! Credits out for req queue (from producers of incoming requests)
    sparta::DataOutPort<uint32_t> req_queue_credits_out_;

    //! Send the initial credits
    void sendInitialCredits_();

    //! Helper function to handle incoming requests
    void handleIncomingReq_();

    //! callback function  called by prefetcher_queue_credits_in_ when credits
    //  comes back from consumer
    void receivePrefetchQueueCredits_(const uint32_t &credits);

    //! Helper function used to generate prefetches
    void generatePrefetch_();

  };

}
