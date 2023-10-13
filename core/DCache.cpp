#include "DCache.hpp"

namespace olympia {
    const char DCache::name[] = "cache";

    DCache::DCache(sparta::TreeNode *node, const CacheParameterSet *p) :
            sparta::Unit(node),
            l1_always_hit_(p->l1_always_hit),
            cache_latency_(p->cache_latency),
            max_mshr_entries_(p->mshr_entries),
            mshr_file_("mshr_file",p->mshr_entries, getClock())
    {
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
        addr_decoder_ = l1_cache_->getAddrDecoder();

        // Pipeline config
        cache_pipeline_.enableCollection(node);
        cache_pipeline_.performOwnUpdates();
        cache_pipeline_.setContinuing(true);

        // Pipeline Handlers
        cache_pipeline_.registerHandlerAtStage
            (static_cast<uint32_t>(PipelineStage::LOOKUP),
             CREATE_SPARTA_HANDLER(DCache, lookupHandler_));

        cache_pipeline_.registerHandlerAtStage
            (static_cast<uint32_t>(PipelineStage::DATA_READ),
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
            //if (cache_hit) {
            //    l1_cache_->touchMRU(*cache_line);
            //}
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

    const uint32_t DCache::mshrLookup_(const MemoryAccessInfoPtr &mem_access_info_ptr) {
        // TODO
        return 0;
    }

    const uint32_t DCache::allocateMSHREntry_(uint64_t addr){
        // TODO
        auto block_address = addr_decoder_->calcBlockAddr(addr);
        // Find free entry in MSHRFile and allocate it
        return 0;
    }
    
    // Handle request from the LSU
    void DCache::getInstsFromLSU_(const MemoryAccessInfoPtr &memory_access_info_ptr){
        // Check if there is an incoming cache refill request from BIU
        if(incoming_cache_refill_) {
            // Cache refill is given priority
            // nack signal is sent to the LSU 
            // LSU will have to replay the instruction
            out_lsu_lookup_nack_.send(0);
        }
        else {
            // Append to Cache Pipeline
            cache_pipeline_.append(memory_access_info_ptr);
        }
    }

    // Incoming Cache Refill request from BIU
    void DCache::getAckFromBIU_(const InstPtr &inst_ptr) {
        incoming_cache_refill_ = true;
        // TODO
    }

    // Cache Pipeline Lookup Stage
    void DCache::lookupHandler_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::LOOKUP);
        const MemoryAccessInfoPtr & mem_access_info_ptr = cache_pipeline_[stage_id];
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        const auto & inst_target_addr = inst_ptr->getRAdr();

        // Check if instruction is a store
        bool is_store_inst = inst_ptr->isStoreInst();
        
        const bool hit_on_cache = cacheLookup_(mem_access_info_ptr);

        if(hit_on_cache){
            mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
            // Send Lookup Ack to LSU and proceed to next stage
            out_lsu_lookup_ack_.send(mem_access_info_ptr);
        }
        else{
          
            // Check MSHR Entries for address match
            auto mshr_idx = mshrLookup_(mem_access_info_ptr);

            // if address matches
            if(mshr_idx < max_mshr_entries_){

                mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);

                if(is_store_inst) {
                    // update Line fill buffer; set valid bit to false TODO
                    // mshr_file_[mshr_idx].setValid(false);
                    mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
                }
                // Send Lookup Ack to LSU and proceed to next stage
                out_lsu_lookup_ack_.send(mem_access_info_ptr);
            }
            else {
                // Check if MSHR entry available
                if(mshr_file_.size() < max_mshr_entries_) {
                    uint32_t mshr_idx = allocateMSHREntry_(inst_target_addr);
                    mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);

                    if(is_store_inst) {
                        // Update Line fill buffer; set dirty bit to false TODO
                        // mshr_file_[mshr_idx].setValid(false);
                        mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
                    }
                    // Send Lookup Ack to LSU and proceed to next stage
                    out_lsu_lookup_ack_.send(mem_access_info_ptr);
                }
                else {
                    // Cache is unable to handle ld/st request
                    out_lsu_lookup_nack_.send();
                    // Drop instruction from pipeline TODO
                }
            }
        }
    }

    // Cache Pipeline Data Read Stage
    void DCache::dataHandler_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::DATA_READ);
        const MemoryAccessInfoPtr & mem_access_info_ptr = cache_pipeline_[stage_id];
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        const auto & inst_target_addr = inst_ptr->getRAdr();

        // Check if instruction is a store
        bool is_store_inst = inst_ptr->isStoreInst();

        if (mem_access_info_ptr->isCacheHit()) {
          
            auto cache_line = l1_cache_->getLine(inst_target_addr);
            if (is_store_inst) {
                // Write to cache line
                cache_line->setValid(false);
            }
            // update replacement information
            l1_cache_->touchMRU(*cache_line);

            mem_access_info_ptr->setDataIsReady(true);

            // Send request back to LSU
            out_lsu_lookup_req_.send(mem_access_info_ptr);
        }
        else {
            // Initiate read to downstream
            uev_issue_downstream_read_.schedule(sparta::Clock::Cycle(0));
        }
    }

    void DCache::issueDownstreamRead_() {
        // TODO
        // If there is no ongoing refill request
        // Issue MSHR Refill request to downstream
    } 
}
