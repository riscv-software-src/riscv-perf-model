#include "DCache.hpp"

namespace olympia {
    const char DCache::name[] = "cache";

    DCache::DCache(sparta::TreeNode *node, const CacheParameterSet *p) :
            sparta::Unit(node),
            mshr_entry_allocator(50, 30),    // 50 and 30 are arbitrary numbers here.  It can be tuned to an exact value.
            l1_always_hit_(p->l1_always_hit),
            cache_line_size_(p->l1_line_size),
            cache_latency_(p->cache_latency),
            max_mshr_entries_(p->mshr_entries),
            mshr_file_("mshr_file",p->mshr_entries, getClock())
    {
        // Port config
        in_lsu_lookup_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(DCache, processInstsFromLSU_, MemoryAccessInfoPtr));

        in_biu_ack_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(DCache, getRefillFromBIU_, InstPtr));

        // DL1 cache config
        const uint32_t l1_size_kb = p->l1_size_kb;
        const uint32_t l1_associativity = p->l1_associativity;
        l1_cache_.reset(new SimpleDL1(getContainer(), l1_size_kb, cache_line_size_, sparta::cache::TreePLRUReplacement(l1_associativity)));
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

    sparta::Buffer<DCache::MSHREntryInfoPtr>::iterator DCache::mshrLookup_(const uint64_t& block_address) {
        sparta::Buffer<MSHREntryInfoPtr>::iterator it;

        for(it = mshr_file_.begin(); it < mshr_file_.end(); it++) {
            auto mshr_block_addr = (*it)->getBlockAddress();
            if(mshr_block_addr == block_address){
                // Address match
                break;
            }
        }

        return it;
    }

    sparta::Buffer<DCache::MSHREntryInfoPtr>::iterator DCache::allocateMSHREntry_(uint64_t block_address){

        MSHREntryInfoPtr mshr_entry = sparta::allocate_sparta_shared_pointer<MSHREntryInfo>(mshr_entry_allocator,
                                                                                            block_address, cache_line_size_);
        auto it = mshr_file_.push_back(mshr_entry);
        return it;
    }
    
    // Handle request from the LSU
    void DCache::processInstsFromLSU_(const MemoryAccessInfoPtr &memory_access_info_ptr){
        // Check if there is an incoming cache refill request from BIU
        if(incoming_cache_refill_) {
            // Cache refill is given priority
            // nack signal is sent to the LSU
            // LSU will have to replay the instruction
            out_lsu_lookup_nack_.send(1);
        }
        else {
            // Append to Cache Pipeline
            cache_pipeline_.append(memory_access_info_ptr); // Data read stage executes in the next cycle
        }
    }

    // Incoming Cache Refill request from BIU
    // Precedence: Before S1 (Cache Lookup)
    void DCache::getRefillFromBIU_(const InstPtr &inst_ptr) {
        // change inst_ptr to cache_line TODO
        incoming_cache_refill_ = true;
        
        // Write cache line to line fill buffer of ongoing mshr entry refill request
        (*current_refill_mshr_entry_)->setDataArrived(true);

        // Proceed to next stage (next cycle)
        uev_cache_refill_write_.schedule(1);
    }

    void DCache::incomingRefillBIU_() {
        // temporary event
        incoming_cache_refill_ = true;
        
        // Write cache line to line fill buffer of ongoing mshr entry refill request
        (*current_refill_mshr_entry_)->setDataArrived(true);

        // Proceed to next stage (next cycle)
        uev_cache_refill_write_.schedule(1);
    }

    // Cache Pipeline Lookup Stage
    void DCache::lookupHandler_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::LOOKUP);
        const MemoryAccessInfoPtr & mem_access_info_ptr = cache_pipeline_[stage_id];
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        const auto & inst_target_addr = inst_ptr->getRAdr();
        const uint64_t block_addr = addr_decoder_->calcBlockAddr(inst_target_addr);

        // Check if instruction is a store
        const bool is_store_inst = inst_ptr->isStoreInst();
        
        const bool hit_on_cache = cacheLookup_(mem_access_info_ptr); // Check both MSHRFile and Cache

        if(hit_on_cache){
            mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
            // Send Lookup Ack to LSU and proceed to next stage
            out_lsu_lookup_ack_.send(mem_access_info_ptr);
        }
        else{
          
            // Check MSHR Entries for address match
            auto mshr_idx = mshrLookup_(block_addr);

            // if address matches
            if(mshr_idx != mshr_file_.end()){

                mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);
                bool data_arrived = (*mshr_idx)->getDataArrived();

                // if LD and data arrived, Hit
                // All ST are considered Hit
                if(data_arrived || is_store_inst) {
                    // Update Line fill buffer only if ST
                    (*mshr_idx)->setModified(is_store_inst);
                    mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
                }

                // Send Lookup Ack to LSU and proceed to next stage
                out_lsu_lookup_ack_.send(mem_access_info_ptr);

            }
            else {
                // Check if MSHR entry available
                if(mshr_file_.numFree() > 0) {
                    auto mshr_idx = allocateMSHREntry_(block_addr);
                    mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);

                    if(is_store_inst) {
                        // Update Line fill buffer
                        (*mshr_idx)->setModified(true);
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

        if  (mem_access_info_ptr->isCacheHit()) {
          
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
        // If there is no ongoing refill request
        // Issue MSHR Refill request to downstream
        if(ongoing_cache_refill_) {
            return;
        }

        // Requests are sent in FIFO order
        current_refill_mshr_entry_ = mshr_file_.begin();

        // Send BIU request
        // out_biu_req_.send(cache_line);
        // Temp fix: call event after fixed time (4 cycles)
        uev_biu_incoming_refill_.schedule(4);
        // Only 1 ongoing refill request at any time
        ongoing_cache_refill_ = true;
    } 

    void DCache::cacheRefillWrite_() {
        incoming_cache_refill_ = false;
        // Write line buffer into cache
        // Initiate write to downstream in case victim line TODO
        auto line_buffer = (*current_refill_mshr_entry_)->getLineFillBuffer();
        auto block_address = (*current_refill_mshr_entry_)->getBlockAddress();
        auto l1_cache_line = &l1_cache_->getLineForReplacementWithInvalidCheck(block_address);

        // Write line buffer data onto cache line
        (*l1_cache_line) = line_buffer;
        l1_cache_->allocateWithMRUUpdate(*l1_cache_line, block_address);

        // Deallocate MSHR entry (after a cycle)
        mshr_file_.erase(current_refill_mshr_entry_);
        ongoing_cache_refill_ = false;
        // reissue downstream read (also after a cycle)
        uev_issue_downstream_read_.schedule(1);

    }
}
