#pragma once

#include "sparta/simulation/Unit.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "sparta/resources/Pipeline.hpp"
#include "SimpleDL1.hpp"
#include "Inst.hpp"
#include "cache/TreePLRUReplacement.hpp"
#include "MemoryAccessInfo.hpp"

namespace olympia {
    class DCache : public sparta::Unit {
    public:
        class CacheParameterSet : public sparta::ParameterSet {
        public:
            CacheParameterSet(sparta::TreeNode *n) : sparta::ParameterSet(n) {}

            // Parameters for the DL1 cache
            PARAMETER(uint32_t, l1_line_size, 64, "DL1 line size (power of 2)")
            PARAMETER(uint32_t, l1_size_kb, 32, "Size of DL1 in KB (power of 2)")
            PARAMETER(uint32_t, l1_associativity, 8, "DL1 associativity (power of 2)")
            PARAMETER(uint32_t, cache_latency, 1, "Assumed latency of the memory system")
            PARAMETER(bool, l1_always_hit, false, "DL1 will always hit")
            PARAMETER(uint32_t, mshr_entries, 8, "Number of MSHR Entries")
        };

        static const char name[];
        DCache(sparta::TreeNode *n, const CacheParameterSet *p);

        enum class PipelineStage
        {
            LOOKUP = 0,
            DATA = 1,
            NUM_STAGES
        };

    private:
        using MSHRFile = std::unordered_map<uint64_t, SimpleCacheLine>;
        MSHRFile mshr_file_;

        using L1Handle = SimpleDL1::Handle;
        L1Handle l1_cache_;
        const bool l1_always_hit_;
        bool busy_;
        uint32_t cache_latency_;
        uint32_t max_mshr_entries_;

        // To keep track of cache refill request
        bool ongoing_cache_refill = false;

        bool cacheLookup_(const MemoryAccessInfoPtr &mem_access_info_ptr);

        void reloadCache_(uint64_t phy_addr);

        void getInstsFromLSU_(const MemoryAccessInfoPtr &memory_access_info_ptr);

        void getAckFromBIU_(const InstPtr &inst_ptr);

        void lookupHandler_();

        void dataHandler_();

        // Cache Pipeline
        sparta::Pipeline<MemoryAccessInfoPtr> cache_pipeline_
            {"CachePipeline", static_cast<uint32_t>(PipelineStage::NUM_STAGES), getClock()};

        ////////////////////////////////////////////////////////////////////////////////
        // Input Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataInPort<MemoryAccessInfoPtr> in_lsu_lookup_req_
                {&unit_port_set_, "in_lsu_lookup_req", 0};

        sparta::DataInPort<InstPtr> in_biu_ack_
                {&unit_port_set_, "in_biu_ack", 1};

        ////////////////////////////////////////////////////////////////////////////////
        // Output Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::SignalOutPort out_lsu_lookup_nack_
                {&unit_port_set_, "out_lsu_lookup_nack", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_lsu_lookup_ack_
                {&unit_port_set_, "out_lsu_lookup_ack", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_lsu_lookup_req_
                {&unit_port_set_, "out_lsu_lookup_req", 1};

        sparta::DataOutPort<InstPtr> out_biu_req_
                {&unit_port_set_, "out_biu_req"};

        ////////////////////////////////////////////////////////////////////////////////
        // Counters
        ////////////////////////////////////////////////////////////////////////////////
        sparta::Counter dl1_cache_hits_{
                getStatisticSet(), "dl1_cache_hits",
                "Number of DL1 cache hits", sparta::Counter::COUNT_NORMAL
        };

        sparta::Counter dl1_cache_misses_{
                getStatisticSet(), "dl1_cache_misses",
                "Number of DL1 cache misses", sparta::Counter::COUNT_NORMAL
        };
    };

}
