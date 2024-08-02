#pragma once
#include "PrefetcherIFTypes.hpp"
#include "MemoryAccessInfo.hpp"

/**
 * @file  PrefetchEngineIF.hpp
 * @brief Prefetcher engine API. This implements the prefetching algorithm
 */

namespace olympia
{

    template <typename PrefetcherStateUpdateType = NullPrefetcherStateUpdateType>
    class PrefetchEngineIF
    {
      public:
        //! /brief Default constructor
        PrefetchEngineIF() = default;

        //! /brief Destructor
        virtual ~PrefetchEngineIF() {}

        /**
         * \brief Checks if a prefetch request is ready
         * \returns true if one or more prefetch requests are ready, false otherwise
         */
        virtual bool isPrefetchReady() const = 0;

        /**
         * \brief Process incoming memory requests (abstract function)
         *
         * \param access Const reference to MemoryAccessPtr (memory request)
         * \returns true if the request is accepted, false otherwise
         */
        virtual bool handleMemoryAccess(const MemoryAccessInfoPtr & access) = 0;

        /**
         * \brief Update the (external) state this prefetcher depends on
         *
         * \param prefetch_state New state update object
         *
         * Default implementation is to do nothing
         *
         */
        virtual void updatePrefetcherState(const PrefetcherStateUpdateType & prefetcher_state)
        {
            (void)prefetcher_state;
        };

        /**
         * \brief Get the next prefetch request (abstract function)
         * \return Memory access to prefetch
         * \note All the attributes for the request are copied from the incoming
         * transactions that caused the current set of prefetches being generated.
         *
         * \returns const copy of MemoryAccesInfosptr
         */
        virtual const MemoryAccessInfoPtr getPrefetchMemoryAccess() const = 0;

        /*!
         * \brief Remove the next prefetch access
         *
         * \note having both getPrefetchMemoryAccess() and popPrefetchMemoryAccess() allows for
         * implementers to have the choice to use a seperate
         * getPrefetchMemoryAccess()/popPrefetchMemoryAccess() or combine both operations in
         * getPrefetchMemoryAccess() and do nothing in popPrefetchMemoryAccess()
         */
        virtual void popPrefetchMemoryAccess() = 0;
    };

} // namespace olympia
