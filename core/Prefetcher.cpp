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
        max_prefetcher_credits_(p->req_queue_size),
        req_queue_("Req_Queue", p->req_queue_size, getClock()),
        ev_gen_prefetch_{getEventSet(), "gen_prefetch_event",
                         CREATE_SPARTA_HANDLER(Prefetcher, generatePrefetch_)},
        ev_handle_incoming_req_{getEventSet(), "handle_incoming_event",
                                CREATE_SPARTA_HANDLER(Prefetcher, handleIncomingReq_)}
    {
        if (prefetcher_enabled_)
        {
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
        }
        else
        {
            p->prefetcher_type.ignore();
            p->num_to_prefetch.ignore();
            p->cacheline_size.ignore();
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



    //! \brief Flush handler
    void Prefetcher::handleFlush(const olympia::FlushManager::FlushingCriteria & criteria)
    {
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

    //! Restore a single prefetch credit (called by DCache on prefetch completion)
    void Prefetcher::restorePrefetchCredit()
    {
        sparta_assert(prefetcher_credits_ < max_prefetcher_credits_,
                      "Attempted to restore prefetch credit beyond max");
        prefetcher_credits_++;

        // If engine has pending prefetches, resume generation
        if (prefetcher_enabled_ && getPrefetchEngine()->isPrefetchReady())
        {
            ev_gen_prefetch_.schedule(sparta::Clock::Cycle(0));
        }
    }

} // namespace olympia
