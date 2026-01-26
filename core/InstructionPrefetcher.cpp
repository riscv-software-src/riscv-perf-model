#include "InstructionPrefetcher.hpp"
#include "NextLinePrefetchEngine.hpp"

namespace olympia
{

    InstructionPrefetcher::InstructionPrefetcher(sparta::TreeNode* node, 
                                                 const InstructionPrefetcherParameterSet* p) :
        sparta::Unit(node),
        PrefetcherIF<PrefetchEngineIF<>>(dynamic_cast<sparta::Unit*>(this)),
        prefetcher_enabled_(p->enable_prefetcher),
        prefetcher_credits_(p->req_queue_size),
        req_queue_("Req_Queue", p->req_queue_size, getClock()),
        ev_gen_prefetch_{getEventSet(), "gen_prefetch_event",
                         CREATE_SPARTA_HANDLER(InstructionPrefetcher, generatePrefetch_)},
        ev_handle_incoming_req_{getEventSet(), "handle_incoming_event",
                                CREATE_SPARTA_HANDLER(InstructionPrefetcher, handleIncomingReq_)},
        prefetcher_queue_credits_in_{getPortSet(), "in_prefetcher_queue_credits",
                                     sparta::SchedulingPhase::Tick, 0},
        req_queue_credits_out_{getPortSet(), "out_req_queue_credit"}
    {
        if (prefetcher_enabled_)
        {
            std::unique_ptr<PrefetchEngineIF<>> prefetch_engine;
            if (p->prefetcher_type == std::string("next_line"))
            {
                prefetch_engine.reset(
                    new NextLinePrefetchEngine(p->num_to_prefetch, p->cacheline_size));
            }
            else
            {
                sparta_assert(!"Invalid prefetcher type specified");
            }
            setEngine(std::move(prefetch_engine));

            prefetcher_queue_credits_in_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(
                InstructionPrefetcher, receivePrefetchQueueCredits_, uint32_t));
        }

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(InstructionPrefetcher, sendInitialCredits_));
    }

    // Send the initial credit count
    void InstructionPrefetcher::sendInitialCredits_()
    {
        req_queue_credits_out_.send(req_queue_.capacity());
    }

    //////////////////////////////////////////////////////////////////////
    // callbacks

    //! Process the incoming instruction fetch access
    void
    InstructionPrefetcher::processIncomingReq(const olympia::MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        // Queue incoming buffer
        req_queue_.push(mem_access_info_ptr);
        ev_handle_incoming_req_.schedule(sparta::Clock::Cycle(0));
    }

    //! Handler for incoming requests
    void InstructionPrefetcher::handleIncomingReq_()
    {
        auto access = req_queue_.read(0);
        req_queue_.pop();
        // Send a credit back
        req_queue_credits_out_.send(1);

        // Only generate prefetches if enabled
        if (prefetcher_enabled_)
        {
            handleMemoryAccess(access);
        }

        if (!req_queue_.empty())
        {
            ev_handle_incoming_req_.schedule(sparta::Clock::Cycle(1));
        }
        if (prefetcher_enabled_ && prefetcher_credits_ > 0)
        {
            ev_gen_prefetch_.schedule(sparta::Clock::Cycle(1));
        }
        return;
    }

    //! \brief Receive prefetcher queue credits
    void InstructionPrefetcher::receivePrefetchQueueCredits_(const uint32_t & credits)
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
    void InstructionPrefetcher::handleFlush(const olympia::FlushManager::FlushingCriteria & criteria)
    {
        req_queue_credits_out_.send(req_queue_.size());
        req_queue_.clear();

        ev_gen_prefetch_.cancel();
        ev_handle_incoming_req_.cancel();
    }

    //! Handle to generate a prefetch
    void InstructionPrefetcher::generatePrefetch_()
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