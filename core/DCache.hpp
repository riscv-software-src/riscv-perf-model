#pragma once

#include "sparta/simulation/Unit.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "CacheFuncModel.hpp"
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
        };

        static const char name[];
        DCache(sparta::TreeNode *n, const CacheParameterSet *p);

    private:
        bool dataLookup_(const MemoryAccessInfoPtr &mem_access_info_ptr);

        void reloadCache_(uint64_t phy_addr);

        void getInstsFromLSU_(const MemoryAccessInfoPtr &memory_access_info_ptr);

        void getAckFromBIU_(const InstPtr &inst_ptr);

        using L1Handle = CacheFuncModel::Handle;
        L1Handle l1_cache_;
        const bool l1_always_hit_;
        bool busy_;
        uint32_t cache_latency_;
        // Keep track of the instruction that causes current outstanding cache miss
        MemoryAccessInfoPtr cache_pending_inst_ = nullptr;

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
        sparta::SignalOutPort out_lsu_free_req_
                {&unit_port_set_, "out_lsu_free_req", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_lsu_lookup_ack_
                {&unit_port_set_, "out_lsu_lookup_ack", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_lsu_lookup_req_
                {&unit_port_set_, "out_lsu_lookup_req", 1};

        sparta::DataOutPort<InstPtr> out_biu_req_
                {&unit_port_set_, "out_biu_req"};

        ////////////////////////////////////////////////////////////////////////////////
        // Events
        ////////////////////////////////////////////////////////////////////////////////

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
