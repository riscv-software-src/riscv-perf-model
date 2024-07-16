// <PrefetcherIF.hpp> -*- C++ -*-
/**
 * \file   PrefetcherIF.h
 *
 * \brief  File that defines a generic Prefetcher API
 */

#pragma once

#include "PrefetcherIFTypes.hpp"
#include "PrefetchEngineIF.hpp"
#include "MemoryAccessInfo.hpp"
#include "FlushManager.hpp"

namespace olympia
{

    /**
     * \class PrefetcherIF
     *
     * \brief Generic Prefetcher API
     *
     * \tparam PrefetchEngineIF Prefetch Engine to use
     * \tparam PrefetcherStateUpdateType State updated type to keep track
               of external state the prefetcher depends upon. Defaulted
     *         to NullPrefetcherStateUpdateType
     *
     * Implementers of Prefetchers can choose one of two methods
     *   a) Derive from this class. In this method you can only
     *      have one prefetch engine at runtime, though a choice
     *      of multiple prefetch engines can be supported
     *   b) Created instances of this (or a derived) classes. In
     *      this method multiple prefetchers with different prefetch
     *      engines can be created
     *
     * Default implementation has little timing. A simple input memory request
     * queue is created to hold incoming memory requests. The requests are
     * immediately sent to the prefetch engine to generate prefetches. The
     * generated prefetches are sent to output port. Flushes are ignored.
     * Count is kept of incoming requests and generated prefetches. If there are
     * multiple incoming requests in a cycle, they are handled one requests per
     * cycle in order of the requests recieved.
     *
     */
    template <typename PrefetchEngineIF,
              typename PrefetcherStateUpdateType = NullPrefetcherStateUpdateType>
    class PrefetcherIF
    {
      public:
        /**
         * \brief Constructor for PrefetcherIF
         *
         * \param Sparta Unit this prefetcher belongs to
         */
        PrefetcherIF(sparta::Unit* unit) :
            in_req_{unit->getPortSet(), "n_req", 0},
            prefetcher_outp_{unit->getPortSet(), "out_prefetcher_write"},
            in_reorder_flush_{unit->getPortSet(), "in_reorder_flush",
                              sparta::SchedulingPhase::Flush, 1},
            cnt_req_rcvd_{unit->getStatisticSet(), "cnt_req_rcvd",
                          "Number of memory requests recieved", sparta::Counter::COUNT_NORMAL},
            cnt_prefetch_snd_{unit->getStatisticSet(), "cnt_prefetch_sent",
                              "Number of prefetch requests sent", sparta::Counter::COUNT_NORMAL},
            unit_(unit)
        {
            in_req_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(
                PrefetcherIF, processIncomingReq_, MemoryAccessInfoPtr));
            in_reorder_flush_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(
                PrefetcherIF, handleFlush_, FlushManager::FlushingCriteria));
        }

        //! Destroy the PrefetcherIF
        virtual ~PrefetcherIF(){};

        /*!
         * \brief Sets the prefetcher engine
         * \param engine Unique ptr to prefetcher engine
         *
         * \note The ownership of engine is transferred to this class once this method is called
         */
        void setEngine(std::unique_ptr<PrefetchEngineIF> engine) { engine_ = std::move(engine); }

        /*!
         * \brief Query the readiness of prefetches
         * \return If a prefetch request is ready, returns true, otherwise returns false
         */
        virtual bool isPrefetchReady() { return engine_->isPrefetchReady(); }

        /*!
         * \brief Incoming memory accesses
         * In this default implementation, Transactions are sent to the engine. All
         * prefetches that are available from the engine are enqueued.
         *
         * \return true if Engine is able to accept the memory access, false otherwise
         */
        virtual bool handleMemoryAccess(const MemoryAccessInfoPtr & access)
        {
            cnt_req_rcvd_++;
            if (engine_->handleMemoryAccess(access))
            {
                while (engine_->isPrefetchReady())
                {
                    sendPrefetch(getPrefetchMemoryAccess());
                    cnt_prefetch_snd_++;
                }
                return true;
            }
            return false;
        }

        /*!
         * \brief Update the external state prefetcher engine depends on
         * \param prefetcher_state Prefetcher state to update
         */
        virtual void updatePrefetcherState(const PrefetcherStateUpdateType & prefetcher_state)
        {
            return engine_->updatePrefetcherState(prefetcher_state);
        }

        /*!
         *\brief Get the next prefetch request
         *\ret const copy of prefetch access
         * Default implementation will get and then remove the next prefetch access from
         * the prefetcher engine
         */
        virtual const MemoryAccessInfoPtr getPrefetchMemoryAccess()
        {
            auto access = engine_->getPrefetchMemoryAccess();
            engine_->popPrefetchMemoryAccess();
            return access;
        }

        /*!
         * \brief Process incoming access
         * This virtual function is called for every incoming memory request
         * Default implementation is to call handleMemoryAccess to actually handle the memory access
         */
        virtual void processIncomingReq(const MemoryAccessInfoPtr & access)
        {
            bool ret = handleMemoryAccess(access);
            sparta_assert(ret == true);
        }

        /*!
         *\brief Flush handler
         * Default implementation ignores the flushes and does nothing
         */
        virtual void handleFlush(const FlushManager::FlushingCriteria & criteria)
        {
            (void)criteria;
            return; // do nothing
        }

        /*!
         * \brief Gets the prefetch engine
         * \return Reference to a unique ptr of prefetcher engine
         */
        std::unique_ptr<PrefetchEngineIF> & getPrefetchEngine() { return engine_; }

        /*!
         * \brief Sends a prefetch on output port
         * \param access Memory access to send
         */
        virtual void sendPrefetch(const MemoryAccessInfoPtr & access)
        {
            prefetcher_outp_.send(access);
        }

      private:
        //! Helper function to generate the next prefetch
        void generatePrefetch_();

        //! Callback function to process incoming access (calls processIncomingReq()))
        void processIncomingReq_(const MemoryAccessInfoPtr & access) { processIncomingReq(access); }

        //! Callback function to process flush (calls handleFlush())
        void handleFlush_(const FlushManager::FlushingCriteria & criteria)
        {
            handleFlush(criteria);
        }

        //! Prefetch Engine behind this interface
        std::unique_ptr<PrefetchEngineIF> engine_;

        ////////////////////////////////////////////////////////////////////////////////
        // Ports
        ////////////////////////////////////////////////////////////////////////////////

        //! Port listening to memory requests
        sparta::DataInPort<MemoryAccessInfoPtr> in_req_;

        //! Out Port for generated prefetches
        sparta::DataOutPort<MemoryAccessInfoPtr> prefetcher_outp_;

        //! For flush
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_;

        ////////////////////////////////////////////////////////////////////////////////
        // Counters
        ////////////////////////////////////////////////////////////////////////////////

        //! Count of requests received
        sparta::Counter cnt_req_rcvd_;

        //! Count of prefetches generated
        sparta::Counter cnt_prefetch_snd_;

        //! Sparta unit to use to house ports and stats
        sparta::Unit* unit_;
    }; // class PrefetcherIF

} // namespace olympia
