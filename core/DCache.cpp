#include "DCache.hpp"

namespace olympia {
    const char DCache::name[] = "cache";

    DCache::DCache(sparta::TreeNode *n, const CacheParameterSet *p) :
            sparta::Unit(n),
            l1_always_hit_(p->l1_always_hit),
            cache_latency_(p->cache_latency) {

        in_lsu_lookup_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(DCache, getInstsFromLSU_, MemoryAccessInfoPtr));

        in_l2cache_ack_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(DCache, getAckFromL2Cache_, uint32_t));

        in_l2cache_resp_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(DCache, getRespFromL2Cache_, InstPtr));

        // DL1 cache config
        const uint32_t l1_line_size = p->l1_line_size;
        const uint32_t l1_size_kb = p->l1_size_kb;
        const uint32_t l1_associativity = p->l1_associativity;
        std::unique_ptr<sparta::cache::ReplacementIF> repl(new sparta::cache::TreePLRUReplacement
                                                                   (l1_associativity));
        l1_cache_.reset(new CacheFuncModel(getContainer(), l1_size_kb, l1_line_size, *repl));
    }

    // Reload cache line
    void DCache::reloadCache_(uint64_t phy_addr)
    {
        auto l1_cache_line = &l1_cache_->getLineForReplacementWithInvalidCheck(phy_addr);
        l1_cache_->allocateWithMRUUpdate(*l1_cache_line, phy_addr);

        ILOG("DCache reload complete!");
    }

    // Access DCache
    bool DCache::dataLookup_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        uint64_t phyAddr = inst_ptr->getRAdr();

        bool cache_hit = false;

        if (l1_always_hit_) {
            cache_hit = true;
        }
        else {
            auto cache_line = l1_cache_->peekLine(phyAddr);
            cache_hit = (cache_line != nullptr) && cache_line->isValid();

            // Update MRU replacement state if DCache HIT
            if (cache_hit) {
                l1_cache_->touchMRU(*cache_line);
            }
        }

        if (l1_always_hit_) {
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

    void DCache::getInstsFromLSU_(const MemoryAccessInfoPtr &memory_access_info_ptr){
        const bool hit = dataLookup_(memory_access_info_ptr);
        if(hit){
            memory_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
        }else{
            memory_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);
            // Poll on dcache_l2cache_credits_ > 0 which means
            // that L2Cache can accept requests from DCache.
            // Provide a corresponsing backpressure mechanism up the pipeline.
            if(!busy_) {
                busy_ = true;
                cache_pending_inst_ = memory_access_info_ptr;
                out_l2cache_req_.send(cache_pending_inst_->getInstPtr());

                // Set the --dcache_l2cache_credits_ here.
            }
        }
        out_lsu_lookup_ack_.send(memory_access_info_ptr);
    }

    void DCache::getRespFromL2Cache_(const InstPtr &inst_ptr) {
        out_lsu_lookup_req_.send(cache_pending_inst_);
        reloadCache_(inst_ptr->getRAdr());
        cache_pending_inst_.reset();
        busy_ = false;
    }

    void DCache::getAckFromL2Cache_(const uint32_t &ack) {
        // When DCache sends the request to L2Cache for a miss,
        // This bool will be set to false, and Dcache should wait for ack from 
        // L2Cache notifying DCache that there is space in it's dcache request buffer
        // 
        // Set it to true so that the following misses from DCache can be sent out to L2Cache. 
        dcache_l2cache_credits_ = ack;
    }

}
