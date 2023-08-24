#pragma once

#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "SimpleDL1.hpp"
#include "Inst.hpp"
#include "cache/TreePLRUReplacement.hpp"
#include "LSU_MMU_Shared.hpp"

namespace olympia {
    class DCache : public sparta::Unit {
    public:
        class CacheParameterSet : public sparta::ParameterSet {
        public:
            CacheParameterSet(sparta::TreeNode *n) : sparta::ParameterSet(n) {}

            // Parameters for the DL1 cache
            PARAMETER(uint32_t, dl1_line_size, 64, "DL1 line size (power of 2)")
            PARAMETER(uint32_t, dl1_size_kb, 32, "Size of DL1 in KB (power of 2)")
            PARAMETER(uint32_t, dl1_associativity, 8, "DL1 associativity (power of 2)")
            PARAMETER(bool, dl1_always_hit, false, "DL1 will always hit")
        };

        DCache(sparta::TreeNode *n, const CacheParameterSet *p);

        bool cacheLookup(const MemoryAccessInfoPtr &mem_access_info_ptr);

        void reloadCache(uint64_t phyAddr);

        static const char name[];
        using DL1Handle = SimpleDL1::Handle;
        DL1Handle dl1_cache_;
        const bool dl1_always_hit_;
        // Keep track of the instruction that causes current outstanding cache miss
        InstPtr cache_pending_inst_ptr_ = nullptr;


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