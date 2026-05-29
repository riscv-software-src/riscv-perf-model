#pragma once

#include <memory>
#include "sparta/memory/AddressTypes.hpp"
#include "sparta/utils/MathUtils.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "cache/TreePLRUReplacement.hpp"
#include "common/RandomReplacement.hpp"
#include "common/LRUReplacement.hpp"
#include "SimpleCacheLine.hpp"
#include "CacheFuncModel.hpp"

namespace olympia_mss {

class L23Cache : public sparta::Unit {

public:

    class L23CacheParameterSet : public sparta::ParameterSet {
    public:
        L23CacheParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

        PARAMETER(uint32_t,    cache_line_size,           64, "Cache line size (power of 2)")
        PARAMETER(uint32_t,    cache_size_kb,            512, "Size of Cache in KB (power of 2)")
        PARAMETER(uint32_t,    cache_associativity,       16, "Cache associativity (power of 2)")
        PARAMETER(std::string, cache_repl_policy,     "PLRU", "Cache replacement policy")
        PARAMETER(bool,        cache_inst_always_hit,  false, "Cache will always hit")
        PARAMETER(bool,        cache_data_always_hit,  false, "Cache will always hit")
    };

    L23Cache(sparta::TreeNode* node, const L23CacheParameterSet* p) :
        sparta::Unit(node),
        cache_line_size_(p->cache_line_size),
        cache_associativity_(p->cache_associativity),
        cache_inst_always_hit_(p->cache_inst_always_hit),
        cache_data_always_hit_(p->cache_data_always_hit),
        cache_repl_policy_(setReplPolicy_(p->cache_repl_policy)),
        cache_func_model_(getContainer(), p->cache_size_kb, p->cache_line_size, *cache_repl_policy_, "L23Cache")
    {
        // Cache can't be too big
        sparta_assert(p->cache_size_kb < 1024 * 1024, "cache_size size too big!");

        // Make sure cache size is multiple of line size
        const uint32_t cache_size_bytes = p->cache_size_kb * 1024;
        sparta_assert(cache_size_bytes % cache_line_size_ == 0, 
                        "Cache size has to be a multiple of cache_line_size");

        // Make sure num lines is multiple of associativity
        const uint32_t num_lines = cache_size_bytes / cache_line_size_;
        sparta_assert(num_lines % cache_associativity_ == 0, 
                        "Number of cache lines should be multiple of associativity");

        // Num sets must be a power of 2
        const uint32_t num_sets = num_lines / cache_associativity_;
        sparta_assert(sparta::utils::is_power_of_2(num_sets), 
                        "Number of sets should be power of 2");
    };

    static constexpr char name[] = "cache";

    uint64_t getCacheLineSize() const { return cache_line_size_; }
    uint64_t getCacheAssociativity() const { return cache_associativity_; }

    bool lookup_mem_acess(const olympia::MemoryAccessInfoPtr& ma_ptr) {

        sparta_assert(ma_ptr, "MemoryAccessInfoPtr can't be null!");
        sparta::memory::addr_t              paddr    = ma_ptr->getPAddr();
        olympia::MemoryAccessInfo::UnitName src_unit = ma_ptr->getSrcUnit();

        bool hit = lookup_addr(paddr, src_unit);

        if (!(cache_inst_always_hit_ && src_unit == olympia::MemoryAccessInfo::UnitName::ICACHE)
         && !(cache_data_always_hit_ && src_unit == olympia::MemoryAccessInfo::UnitName::LSU)
         && !(cache_data_always_hit_ && src_unit == olympia::MemoryAccessInfo::UnitName::L2CACHE)
         && hit) {
            // Get a cacheline to update
            auto* cache_line = cache_func_model_.getLine(paddr);

            // if it is a store, we mark it modified
            if (ma_ptr->getReqType() == olympia::MemoryAccessInfo::RequestType::WRITE) {
                cache_line->setModified(true);
            }

            // Update MRU replacement state if hit and mark as modified
            cache_func_model_.touchMRU(*cache_line);
        }
        return hit;
    }

    bool lookup_addr(const sparta::memory::addr_t& paddr, const olympia::MemoryAccessInfo::UnitName& src_unit) {

        bool hit = false;

        if ((cache_inst_always_hit_ && src_unit == olympia::MemoryAccessInfo::UnitName::ICACHE)
         || (cache_data_always_hit_ && src_unit == olympia::MemoryAccessInfo::UnitName::LSU)
         || (cache_data_always_hit_ && src_unit == olympia::MemoryAccessInfo::UnitName::L2CACHE)) {
            hit = true;
        } else {
            const auto* cache_line = cache_func_model_.peekLine(paddr);
            hit = (cache_line != nullptr) && cache_line->isValid();

            // Update MRU replacement state if hit
            if (hit) {
                cache_func_model_.touchMRU(*cache_line);
            }
        }
        return hit;
    }

    std::tuple<bool, bool, sparta::memory::addr_t> reload_mem_access(const olympia::MemoryAccessInfoPtr& ma_ptr) {

        // Get a cacheline to be replaced
        sparta::memory::addr_t paddr = ma_ptr->getPAddr();
        auto cache_line = &cache_func_model_.getLineForReplacementWithInvalidCheck(paddr);

        // Find the eviction if the line is modified
        const auto [is_valid, is_modified, evict_cl_addr] = getEviction(*cache_line);

        // Allocate a new cacheline
        cache_func_model_.allocateWithMRUUpdate(*cache_line, paddr);

        // Get a cacheline that was written to modify it
        // if it is a store, we mark it modified
        auto* written_cl = cache_func_model_.getLine(paddr);

        if (ma_ptr->getReqType() == olympia::MemoryAccessInfo::RequestType::WRITE) {
            written_cl->setModified(true);
        }

        return {is_valid, is_modified, evict_cl_addr};
    }

    std::tuple<bool, bool, sparta::memory::addr_t> getEviction(const olympia::SimpleCacheLine& cache_line) {

        const bool& evicted_cl_valid = cache_line.isValid();
        const bool& evicted_cl_modified = cache_line.isModified();
        sparta::memory::addr_t evicted_cl_addr = cache_line.getAddr();

        return {evicted_cl_valid, evicted_cl_modified, evicted_cl_addr};
    }

private:
    const uint64_t cache_line_size_;
    const uint64_t cache_associativity_;
    const bool     cache_inst_always_hit_;
    const bool     cache_data_always_hit_;

    std::unique_ptr<sparta::cache::ReplacementIF> cache_repl_policy_;
    olympia::CacheFuncModel<olympia::SimpleCacheLine> cache_func_model_;

    // Function to set the replacement policy through the parameters
    std::unique_ptr<sparta::cache::ReplacementIF> setReplPolicy_(std::string repl_policy) {
        
        if (repl_policy == "PLRU") {
            return std::make_unique<sparta::cache::TreePLRUReplacement>(cache_associativity_);
        }
        else if (repl_policy == "LRU") {
            return std::make_unique<sparta::cache::LRUReplacement>(cache_associativity_);
        }
        else if (repl_policy == "RANDOM") {
            return std::make_unique<sparta::cache::RandomReplacement>(cache_associativity_);
        }
        else if (repl_policy == "NRU") {
            // to be implemented
        }
        else if (repl_policy == "RRIP") {
            // to be implemented
        }
        else {
            sparta_assert(false, "Invalid Replacement policy for L23Cache");
        }

        return nullptr;
    }
};

} // namespace olympia_mss
