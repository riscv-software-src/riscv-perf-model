// <PrefetcherIFTypes.hpp> -*- C++ -*-
/**
 * \file   PrefetcherIFTypes.hpp
 *
 * \brief  Common Types used in Prefetchers
 */

#pragma once

namespace olympia
{

    /**
     * \class NullPrefetcherStateUpdateType
     *
     * \brief Default prefetcher statue update type, used
     *        as a template type for PrefetcherIF
     *
     * Default state update class used as function parameter for simple prefetchers
     * without outside (of prefetcher) dependencies.
     */
    class NullPrefetcherStateUpdateType
    {
      public:
        //! \brief default constructor
        NullPrefetcherStateUpdateType() = default;
    }; // class NullPrefetcherStateUpdateType

} // namespace olympia