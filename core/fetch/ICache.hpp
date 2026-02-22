// <Fetch.hpp> -*- C++ -*-

//!
//! \file Fetch.hpp
//! \brief Definition of the CoreModel ICache unit
//!

#pragma once

#include "sparta/simulation/Unit.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/resources/Buffer.hpp"
#include "sparta/utils/LogUtils.hpp"

#include "memory/CacheFuncModel.hpp"
#include "inst/Inst.hpp"
#include "cache/TreePLRUReplacement.hpp"
#include "memory/MemoryAccessInfo.hpp"

namespace olympia
{

    /**
     * @file   ICache.hpp
     * @brief The L1 Instruction Cache block -- L1 Cache unit for instruction code
     *
     * This is an L1 Instruction Cache that features:
     *   - Hit and miss under miss
     *   - Pipelining of requests
     *   - Automatic miss replay following a linefill
     * Both interfaces use a credit protocol.
     */
    class ICache : public sparta::Unit
    {
      public:
        class ICacheParameterSet : public sparta::ParameterSet
        {
          public:
            ICacheParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

            // Parameters for the IL1 cache
            PARAMETER(uint32_t, l1_line_size, 64, "IL1 line size (power of 2)")
            PARAMETER(uint32_t, l1_size_kb, 32, "Size of IL1 in KB (power of 2)")
            PARAMETER(uint32_t, l1_associativity, 8, "IL1 associativity (power of 2)")
            PARAMETER(std::string, replacement_policy, "TreePLRU", "IL1 cache replacement policy")
            PARAMETER(uint32_t, cache_latency, 1, "Assumed latency of the memory system")
            PARAMETER(bool, l1_always_hit, false, "IL1 will always hit")
        };

        static const char name[];
        ICache(sparta::TreeNode* n, const ICacheParameterSet* p);

      private:

        void doArbitration_();
        void sendReplay_(const MemoryAccessInfoPtr &);
        void sendResponse_(const MemoryAccessInfoPtr &);
        void addToMissQueue_(const MemoryAccessInfoPtr &);
        void makeL2CacheRequest_();
        void reloadCache_(const MemoryAccessInfoPtr&);
        bool lookupCache_(const MemoryAccessInfoPtr &);

        void sendInitialCredits_();

        // Callbacks
        void getRequestFromFetch_(const MemoryAccessInfoPtr &);
        void getCreditsFromL2Cache_(const uint32_t &);
        void getRespFromL2Cache_(const MemoryAccessInfoPtr &);

        using L1Handle = CacheFuncModel::Handle;
        L1Handle l1_cache_;
        const bool l1_always_hit_;
        const uint32_t cache_latency_ = 0;
        const uint32_t fetch_queue_size_ = 8;

        std::deque<MemoryAccessInfoPtr> l2cache_resp_queue_;
        std::deque<MemoryAccessInfoPtr> fetch_req_queue_;
        std::deque<MemoryAccessInfoPtr> replay_buffer_;
        std::deque<MemoryAccessInfoPtr> miss_queue_;

        sparta::Buffer<MemoryAccessInfoPtr> pending_miss_buffer_;

        // Credits for sending miss request to L2Cache
        uint32_t l2cache_credits_ = 0;

        olympia::MemoryAccessInfoAllocator & memory_access_allocator_;

        ////////////////////////////////////////////////////////////////////////////////
        // Input Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataInPort<MemoryAccessInfoPtr> in_fetch_req_{&unit_port_set_,
                                                             "in_fetch_req", 1};

        sparta::DataInPort<uint32_t> in_l2cache_credits_{&unit_port_set_, "in_l2cache_credits", 1};

        sparta::DataInPort<MemoryAccessInfoPtr> in_l2cache_resp_{&unit_port_set_,
                                                                 "in_l2cache_resp", 1};

        ////////////////////////////////////////////////////////////////////////////////
        // Output Ports
        ////////////////////////////////////////////////////////////////////////////////

        sparta::DataOutPort<uint32_t> out_fetch_credit_{&unit_port_set_,
                                                        "out_fetch_credit", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_fetch_resp_{&unit_port_set_,
                                                                "out_fetch_resp", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_l2cache_req_{&unit_port_set_,
                                                                  "out_l2cache_req", 0};

        ////////////////////////////////////////////////////////////////////////////////
        // Events
        ////////////////////////////////////////////////////////////////////////////////
        sparta::UniqueEvent<> ev_arbitrate_{&unit_event_set_, "ev_arbitrate",
                CREATE_SPARTA_HANDLER(ICache, doArbitration_)};

        sparta::UniqueEvent<> ev_l2cache_request_{&unit_event_set_, "ev_l2cache_request",
                CREATE_SPARTA_HANDLER(ICache, makeL2CacheRequest_)};


        sparta::PayloadEvent<MemoryAccessInfoPtr> ev_respond_{
            &unit_event_set_, "ev_respond",
            CREATE_SPARTA_HANDLER_WITH_DATA(ICache, sendResponse_, MemoryAccessInfoPtr)};

        sparta::PayloadEvent<MemoryAccessInfoPtr> ev_replay_ready_{
            &unit_event_set_, "ev_replay_ready",
            CREATE_SPARTA_HANDLER_WITH_DATA(ICache, sendReplay_, MemoryAccessInfoPtr)};

        ////////////////////////////////////////////////////////////////////////////////
        // Counters
        ////////////////////////////////////////////////////////////////////////////////
        sparta::Counter il1_cache_hits_{getStatisticSet(), "IL1_cache_hits",
                                        "Number of IL1 cache hits", sparta::Counter::COUNT_NORMAL};

        sparta::Counter il1_cache_misses_{getStatisticSet(), "IL1_cache_misses",
                                          "Number of IL1 cache misses",
                                          sparta::Counter::COUNT_NORMAL};

        friend class ICacheTester;
    };
    class ICacheTester;
} // namespace olympia
