#include "Prefetcher.hpp"
#include "NextLinePrefetchEngine.hpp"
#include "StridePrefetchEngine.hpp"

namespace olympia
{

    Prefetcher::Prefetcher(sparta::TreeNode* node, 
                           const PrefetcherParameterSet* p) :
        sparta::Unit(node),
        PrefetcherIF<PrefetchEngineIF<>>(dynamic_cast<sparta::Unit*>(this)),
        prefetcher_enabled_(p->enable_prefetcher),
        prefetcher_credits_(p->req_queue_size),
        req_queue_("Req_Queue", p->req_queue_size, getClock()),
        ev_gen_prefetch_{getEventSet(), "gen_prefetch_event",
                         CREATE_SPARTA_HANDLER(Prefetcher, generatePrefetch_)},
        ev_handle_incoming_req_{getEventSet(), "handle_incoming_event",
                                CREATE_SPARTA_HANDLER(Prefetcher, handleIncomingReq_)}
    {
        if (prefetcher_enabled_)
        {
            prefetcher_queue_credits_in_.reset(new sparta::DataInPort<uint32_t>(getPortSet(), "in_prefetcher_queue_credits",
                                                                                sparta::SchedulingPhase::Tick, 0));
            req_queue_credits_out_.reset(new sparta::DataOutPort<uint32_t>(getPortSet(), "out_req_queue_credit"));

            std::unique_ptr<PrefetchEngineIF<>> prefetch_engine;
            if (p->prefetcher_type == std::string("next_line"))
            {
                prefetch_engine.reset(
                    new NextLinePrefetchEngine(p->num_to_prefetch, p->cacheline_size));
            }
            else if (p->prefetcher_type == std::string("stride"))
            {
                prefetch_engine.reset(
                    new StridePrefetchEngine(p->num_to_prefetch, p->cacheline_size));
            }
            else
            {
                sparta_assert(!"Invalid prefetcher type specified");
            }
            setEngine(std::move(prefetch_engine));

            prefetcher_queue_credits_in_->registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(
                Prefetcher, receivePrefetchQueueCredits_, uint32_t));
        }
        else
        {
            p->prefetcher_type.ignore();
            p->num_to_prefetch.ignore();
            p->cacheline_size.ignore();
        }

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Prefetcher, sendInitialCredits_));
    }

    // Send the initial credit count
    void Prefetcher::sendInitialCredits_()
    {
        if (prefetcher_enabled_)
        {
            req_queue_credits_out_->send(req_queue_.capacity());
        }
    }

    //////////////////////////////////////////////////////////////////////
    // callbacks

    //! Process the incoming memory access
    void
    Prefetcher::processIncomingReq(const olympia::MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        // Queue incoming buffer
        req_queue_.push(mem_access_info_ptr);
        ev_handle_incoming_req_.schedule(sparta::Clock::Cycle(0));
    }

    //! \brief Override handleMemoryAccess to use credit-based flow control
    //! Feeds the engine without immediately sending prefetches.
    //! Prefetches are sent via generatePrefetch_() which checks credits.
    bool Prefetcher::handleMemoryAccess(const MemoryAccessInfoPtr & access)
    {
        if (getPrefetchEngine()->handleMemoryAccess(access))
        {
            // Don't send prefetches immediately — schedule credit-based generation
            if (prefetcher_credits_ > 0)
            {
                ev_gen_prefetch_.schedule(sparta::Clock::Cycle(0));
            }
            return true;
        }
        return false;
    }

    //! Handler for incoming requests
    void Prefetcher::handleIncomingReq_()
    {
        auto access = req_queue_.read(0);
        req_queue_.pop();
        // Send a credit back
        if (prefetcher_enabled_)
        {
            req_queue_credits_out_->send(1);
        }

        // Only generate prefetches if enabled
        if (prefetcher_enabled_)
        {
            handleMemoryAccess(access);
        }

        if (!req_queue_.empty())
        {
            ev_handle_incoming_req_.schedule(sparta::Clock::Cycle(1));
        }
        return;
    }

    //! \brief Receive prefetcher queue credits
    void Prefetcher::receivePrefetchQueueCredits_(const uint32_t & credits)
    {
        prefetcher_credits_ += credits;
        if (prefetcher_enabled_ && getPrefetchEngine()->isPrefetchReady())
        {
            // Cancel any future possible event
            ev_gen_prefetch_.cancel();
            // Generate the prefetch in current cycle
            ev_gen_prefetch_.schedule(sparta::Clock::Cycle(0));
        }
    }

    //! \brief Flush handler
    void Prefetcher::handleFlush(const olympia::FlushManager::FlushingCriteria & criteria)
    {
        if (prefetcher_enabled_)
        {
            req_queue_credits_out_->send(req_queue_.size());
        }
        req_queue_.clear();

        ev_gen_prefetch_.cancel();
        ev_handle_incoming_req_.cancel();
    }

    //! Handle to generate a prefetch
    void Prefetcher::generatePrefetch_()
    {
        if (!prefetcher_enabled_)
        {
            return;
        }

        if (getPrefetchEngine()->isPrefetchReady() && (prefetcher_credits_ > 0))
        {
            // Get next prefetch from the engine
            auto access = getPrefetchEngine()->getPrefetchMemoryAccess();
            getPrefetchEngine()->popPrefetchMemoryAccess();

            // Send access to output port and consume credit
            sendPrefetch(access);
            prefetcher_credits_--;

            // Schedule next prefetch generation if more are ready and we have credits
            if (getPrefetchEngine()->isPrefetchReady() && (prefetcher_credits_ > 0))
            {
                ev_gen_prefetch_.schedule(sparta::Clock::Cycle(1));
            }
        }
    }

} // namespace olympia
