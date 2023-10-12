#include "DCache.hpp"

namespace olympia {
    const char DCache::name[] = "cache";

    DCache::DCache(sparta::TreeNode *node, const CacheParameterSet *p) :
            sparta::Unit(node),
            l1_always_hit_(p->l1_always_hit),
            cache_latency_(p->cache_latency),
            max_mshr_entries_(p->mshr_entries)
    {
        // Pipeline config
        cache_pipeline_.enableCollection(node);
        cache_pipeline_.performOwnUpdates();
        cache_pipeline_.setContinuing(true);

        // Port config
        in_lsu_lookup_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(DCache, getInstsFromLSU_, MemoryAccessInfoPtr));

        in_biu_ack_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(DCache, getAckFromBIU_, InstPtr));

        // DL1 cache config
        const uint32_t l1_line_size = p->l1_line_size;
        const uint32_t l1_size_kb = p->l1_size_kb;
        const uint32_t l1_associativity = p->l1_associativity;
        std::unique_ptr<sparta::cache::ReplacementIF> repl(new sparta::cache::TreePLRUReplacement
                                                                   (l1_associativity));
        l1_cache_.reset(new SimpleDL1(getContainer(), l1_size_kb, l1_line_size, *repl));

        // Pipeline Handlers
        cache_pipeline_.registerHandlerAtStage
            (static_cast<uint32_t>(PipelineStage::LOOKUP),
             CREATE_SPARTA_HANDLER(DCache, lookupHandler_));

        cache_pipeline_.registerHandlerAtStage
            (static_cast<uint32_t>(PipelineStage::DATA),
             CREATE_SPARTA_HANDLER(DCache, dataHandler_));
    }

    // Reload cache line
    void DCache::reloadCache_(uint64_t phy_addr)
    {
        auto l1_cache_line = &l1_cache_->getLineForReplacementWithInvalidCheck(phy_addr);
        l1_cache_->allocateWithMRUUpdate(*l1_cache_line, phy_addr);

        ILOG("DCache reload complete!");
    }

    // Access DCache
    bool DCache::cacheLookup_(const MemoryAccessInfoPtr & mem_access_info_ptr)
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

    // Handle request from the LSU
    void DCache::getInstsFromLSU_(const MemoryAccessInfoPtr &memory_access_info_ptr){
       
        // Check if there is a cache refill request
        if(ongoing_cache_refill) {
            // Cache refill is given priority
            // nack signal is sent to the LSU 
            // LSU will have to replay the instruction
            out_lsu_lookup_nack_.send(1);
        }
        else {
            // Append to Cache Pipeline
            cache_pipeline_.append(memory_access_info_ptr);
        }
    }

    // Incoming Cache Refill request from BIU
    void DCache::getAckFromBIU_(const InstPtr &inst_ptr) {
        reloadCache_(inst_ptr->getRAdr());
    }
    
    // Cache Pipeline Lookup Stage
    void DCache::lookupHandler_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::LOOKUP);
        const MemoryAccessInfoPtr & mem_access_info_ptr = cache_pipeline_[stage_id];
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        const auto & inst_addr = inst_ptr->getRAdr(); // Address of LD/ST inst 

        const auto & addr_decoder = l1_cache_->getAddrDecoder();
        const uint64_t cache_line_address = addr_decoder->calcBlockAddr(inst_addr);

        // Check if instruction is a store
        bool is_store_inst = inst_ptr->isStoreInst();
        
        const bool hit = cacheLookup_(mem_access_info_ptr);

        // Check if hit on MSHR entries
        const bool hit_on_mshr_entries = (mshr_file_.find(cache_line_address) != mshr_file_.end());

        if(hit){
            mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
            // Send Lookup Ack to LSU and proceed to next stage
            out_lsu_lookup_ack_.send(mem_access_info_ptr);

        }else{
            // Check MSHR Entries for address match
            if(hit_on_mshr_entries){
                if(is_store_inst) {
                    // update Line fill buffer
                }
                mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);
                // Send Lookup Ack to LSU and proceed to next stage
                out_lsu_lookup_ack_.send(mem_access_info_ptr);
            }
            else {
                // Check if MSHR entry available
                if(mshr_file_.size() != max_mshr_entries_) {
                    // Allocate MSHR entry

                    if(is_store_inst) {
                        // Update Line fill buffer
                    }
                    mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);
                    // Send Lookup Ack to LSU and proceed to next stage
                    out_lsu_lookup_ack_.send(mem_access_info_ptr);
                }
                else {
                    out_lsu_lookup_nack_.send(0);
                }
            }
        }
    }
    
    void DCache::dataHandler_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::DATA);
        const MemoryAccessInfoPtr & mem_access_info_ptr = cache_pipeline_[stage_id];
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        const auto & inst_addr = inst_ptr->getRAdr(); // Address of LD/ST inst 

        const auto & addr_decoder = l1_cache_->getAddrDecoder();
        const uint64_t cache_line_address = addr_decoder->calcBlockAddr(inst_addr);

        // Check if instruction is a store
        bool is_store_inst = inst_ptr->isStoreInst();

        if (mem_access_info_ptr->isCacheHit()) {
            if (is_store_inst) {
                // Update Cache Line
            }
            // update replacement information

            mem_access_info_ptr->setDataIsReady(true);
        }
        else {
            // Initiate read from downstream
        }

        // Send request back to LSU
        out_lsu_lookup_req_.send(mem_access_info_ptr);
    }
}
