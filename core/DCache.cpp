#include "DCache.hpp"

namespace olympia {
    const char DCache::name[] = "cache";

    DCache::DCache(sparta::TreeNode *n, const CacheParameterSet *p) :
            sparta::Unit(n),
            dl1_always_hit_(p->dl1_always_hit) {
        // DL1 cache config
        const uint32_t dl1_line_size = p->dl1_line_size;
        const uint32_t dl1_size_kb = p->dl1_size_kb;
        const uint32_t dl1_associativity = p->dl1_associativity;
        std::unique_ptr<sparta::cache::ReplacementIF> repl(new sparta::cache::TreePLRUReplacement
                                                                   (dl1_associativity));
        dl1_cache_.reset(new SimpleDL1(getContainer(), dl1_size_kb, dl1_line_size, *repl));
    }

    // Reload cache line
    void DCache::reloadCache(uint64_t phyAddr)
    {
        auto dl1_cache_line = &dl1_cache_->getLineForReplacementWithInvalidCheck(phyAddr);
        dl1_cache_->allocateWithMRUUpdate(*dl1_cache_line, phyAddr);

        ILOG("DCache reload complete!");
    }

    // Access DCache
    bool DCache::cacheLookup(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        uint64_t phyAddr = inst_ptr->getRAdr();

        bool cache_hit = false;

        if (dl1_always_hit_) {
            cache_hit = true;
        }
        else {
            auto cache_line = dl1_cache_->peekLine(phyAddr);
            cache_hit = (cache_line != nullptr) && cache_line->isValid();

            // Update MRU replacement state if DCache HIT
            if (cache_hit) {
                dl1_cache_->touchMRU(*cache_line);
            }
        }

        if (dl1_always_hit_) {
            ILOG("DL1 DCache HIT all the time: phyAddr=0x" << std::hex << phyAddr);
            dl1_cache_hits_++;
        }
        else if (cache_hit) {
            ILOG("DL1 DCache HIT: phyAddr=0x" << std::hex << phyAddr);
            dl1_cache_hits_++;
        }
        else {
            ILOG("DL1 DCache MISS: phyAddr=0x" << std::hex << phyAddr);
            dl1_cache_misses_++;
        }

        return cache_hit;
    }
}