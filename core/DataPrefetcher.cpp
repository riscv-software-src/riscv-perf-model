#include "DataPrefetcher.hpp"
#include "NextLinePrefetchEngine.hpp"

namespace olympia {


DataPrefetcher::DataPrefetcher(sparta::TreeNode * node,
                                   const DataPrefetcherParameterSet * p):
  sparta::Unit(node),
  PrefetcherIF<PrefetchEngineIF<>>(dynamic_cast<sparta::Unit*>(this)),
  prefetcher_credits_(p->req_queue_size),
  req_queue_("Req_Queue", p->req_queue_size, getClock()),
  ev_gen_prefetch_  {getEventSet(), "gen_prefetch_event", CREATE_SPARTA_HANDLER(DataPrefetcher, generatePrefetch_)},
  ev_handle_incoming_req_  {getEventSet(), "handle_incoming_event", CREATE_SPARTA_HANDLER(DataPrefetcher, handleIncomingReq_)},
  prefetcher_queue_credits_in_{getPortSet(), "in_prefetcher_queue_credits", sparta::SchedulingPhase::Tick, 0},
  req_queue_credits_out_{getPortSet(), "out_req_queue_credit"}
{
  std::unique_ptr<PrefetchEngineIF<>> prefetch_engine;
  if (p->prefetcher_type == std::string("next_line"))
    {
      prefetch_engine.reset(new NextLinePrefetchEngine(p->num_to_prefetch, p->cacheline_size));
    }
  else
    {
      sparta_assert(!"Invalid prefetcher type specified");
    }
  setEngine(std::move(prefetch_engine));

  prefetcher_queue_credits_in_.
    registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(DataPrefetcher, receivePrefetchQueueCredits_, uint32_t));

  sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(DataPrefetcher, sendInitialCredits_));
    }


  // Send fetch the initial credit count
  void DataPrefetcher::sendInitialCredits_() 
{
  req_queue_credits_out_.send(req_queue_.capacity());
}

  //////////////////////////////////////////////////////////////////////
  // callbacks

    //! Process the incoming memory access and send to the engine
    void DataPrefetcher::processIncomingReq(const olympia::MemoryAccessInfoPtr& mem_access_info_ptr)
    {
      // queue in incoming buffer
      req_queue_.push(mem_access_info_ptr);
      ev_handle_incoming_req_.schedule(sparta::Clock::Cycle(0));
    }

    //! handler for incoming requests
    void DataPrefetcher::handleIncomingReq_()
    {
      auto access = req_queue_.read(0);
      req_queue_.pop();
      // send a credit back
      req_queue_credits_out_.send(1);

      handleMemoryAccess(access);

      if (!req_queue_.empty()) {
        ev_handle_incoming_req_.schedule(sparta::Clock::Cycle(1));
      }
      if (prefetcher_credits_ > 0) {
        ev_gen_prefetch_.schedule(sparta::Clock::Cycle(1));
      }
      return;
    }

    //! \brief Recieve prefetcher queue Credits  
    void DataPrefetcher::receivePrefetchQueueCredits_(const uint32_t &credits) 
    {
      prefetcher_credits_ += credits;
      if (getPrefetchEngine()->isPrefetchReady()) {
        // cancel any future possible event
        ev_gen_prefetch_.cancel();
        // generate the prefetch in current cycle
        ev_gen_prefetch_.schedule(sparta::Clock::Cycle(0));
      }
    }

    //! \brief Flush handler
    void DataPrefetcher::handleFlush(const olympia::FlushManager::FlushingCriteria & criteria)
    {
      req_queue_credits_out_.send(req_queue_.size());
      req_queue_.clear();

      ev_gen_prefetch_.cancel();
      ev_handle_incoming_req_.cancel();
    }


    //! handle to generate a prefetch
    void DataPrefetcher::generatePrefetch_()
    {
      if (getPrefetchEngine()->isPrefetchReady()) {
        // get next prefetch from the engine
        auto access = getPrefetchEngine()->getPrefetchMemoryAccess();
        getPrefetchEngine()->popPrefetchMemoryAccess();

        // send access to output port and consume credit
        sendPrefetch(access);
        prefetcher_credits_--;

        // schedule next prefetch generation (if any)
        if (getPrefetchEngine()->isPrefetchReady() && (prefetcher_credits_ > 0)) {
          ev_gen_prefetch_.schedule(sparta::Clock::Cycle(1));
        }
      }
    }


  
} // namespace prefetcher
