#include "DCache.hpp"
#include "OlympiaAllocators.hpp"

namespace olympia
{
    const char DCache::name[] = "cache";

    DCache::DCache(sparta::TreeNode* n, const CacheParameterSet* p) :
        sparta::Unit(n),
        l1_always_hit_(p->l1_always_hit),
        cache_latency_(p->cache_latency),
        cache_line_size_(p->l1_line_size),
        num_mshr_entries_(p->mshr_entries),
        mshr_file_("mshr_file", p->mshr_entries, getClock()),
        mshr_entry_allocator_(
            sparta::notNull(OlympiaAllocators::getOlympiaAllocators(n))->mshr_entry_allocator)
    {
        sparta_assert(num_mshr_entries_ > 0, "There must be atleast 1 MSHR entry");

        in_lsu_lookup_req_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(DCache, receiveMemReqFromLSU_, MemoryAccessInfoPtr));

        in_l2cache_ack_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(DCache, receiveAckFromL2Cache_, uint32_t));

        in_l2cache_resp_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(DCache, receiveRespFromL2Cache_, MemoryAccessInfoPtr));

        setupL1Cache_(p);

        // Pipeline config
        cache_pipeline_.enableCollection(n);
        cache_pipeline_.performOwnUpdates();
        cache_pipeline_.setContinuing(true);

        // Pipeline Handlers
        cache_pipeline_.registerHandlerAtStage(static_cast<uint32_t>(PipelineStage::LOOKUP),
                                               CREATE_SPARTA_HANDLER(DCache, handleLookup_));

        cache_pipeline_.registerHandlerAtStage(static_cast<uint32_t>(PipelineStage::DATA_READ),
                                               CREATE_SPARTA_HANDLER(DCache, handleDataRead_));

        cache_pipeline_.registerHandlerAtStage(static_cast<uint32_t>(PipelineStage::DEALLOCATE),
                                               CREATE_SPARTA_HANDLER(DCache, handleDeallocate_));

        mshr_file_.enableCollection(n);
    }

    void DCache::setupL1Cache_(const CacheParameterSet* p)
    { // DL1 cache config
        const uint32_t l1_line_size = p->l1_line_size;
        const uint32_t l1_size_kb = p->l1_size_kb;
        const uint32_t l1_associativity = p->l1_associativity;
        std::unique_ptr<sparta::cache::ReplacementIF> repl(
            new sparta::cache::TreePLRUReplacement(l1_associativity));
        l1_cache_.reset(new CacheFuncModel(getContainer(), l1_size_kb, l1_line_size, *repl));
        addr_decoder_ = l1_cache_->getAddrDecoder();
    }

    // Reload cache line
    void DCache::reloadCache_(uint64_t phy_addr)
    {
        auto l1_cache_line = &l1_cache_->getLineForReplacementWithInvalidCheck(phy_addr);
        l1_cache_->allocateWithMRUUpdate(*l1_cache_line, phy_addr);

        ILOG("DCache reload complete!");
    }

    // Access L1Cache
    bool DCache::dataLookup_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        uint64_t phyAddr = inst_ptr->getRAdr();

        bool cache_hit = false;

        if (l1_always_hit_)
        {
            cache_hit = true;
        }
        else
        {
            auto cache_line = l1_cache_->peekLine(phyAddr);
            cache_hit = (cache_line != nullptr) && cache_line->isValid();

            // Update MRU replacement state if DCache HIT
            if (cache_hit)
            {
                l1_cache_->touchMRU(*cache_line);
            }
        }

        if (l1_always_hit_)
        {
            ILOG("DL1 DCache HIT all the time: phyAddr=0x" << std::hex << phyAddr);
            dl1_cache_hits_++;
        }
        else if (cache_hit)
        {
            ILOG("DL1 DCache HIT: phyAddr=0x" << std::hex << phyAddr);
            dl1_cache_hits_++;
        }
        else
        {
            ILOG("DL1 DCache MISS: phyAddr=0x" << std::hex << phyAddr);
            dl1_cache_misses_++;
        }

        return cache_hit;
    }

    // The lookup stage
    void DCache::handleLookup_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::LOOKUP);
        const MemoryAccessInfoPtr & mem_access_info_ptr = cache_pipeline_[stage_id];
        ILOG(mem_access_info_ptr << " in Lookup stage");
        // If the mem request is a refill we dont do anything in the lookup stage
        if (mem_access_info_ptr->isRefill())
        {
            ILOG("Incoming cache refill " << mem_access_info_ptr);
            return;
        }

        const bool hit = dataLookup_(mem_access_info_ptr);
        ILOG(mem_access_info_ptr << " performing lookup " << hit);
        if (hit)
        {
            mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
            out_lsu_lookup_ack_.send(mem_access_info_ptr);
            return;
        }

        // Check MSHR Entries for address match
        const auto & mshr_itb = mem_access_info_ptr->getMSHRInfoIterator();

        if (!mshr_itb.isValid() && mshr_file_.numFree() == 0)
        {
            // Should be Nack but miss should work for now
            mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);
            out_lsu_lookup_ack_.send(mem_access_info_ptr);
            return;
        }

        if (!mshr_itb.isValid())
        {
            //            mshrLookup_(mem_access_info_ptr);
            if (!mem_access_info_ptr->getMSHRInfoIterator().isValid())
            {
                ILOG("Creating new MSHR Entry " << mem_access_info_ptr);
                allocateMSHREntry_(mem_access_info_ptr);
            }
        }

        replyLSU_(mem_access_info_ptr);
    }

    void DCache::replyLSU_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const auto & mshr_it = mem_access_info_ptr->getMSHRInfoIterator();
        const uint64_t block_addr = getBlockAddr(mem_access_info_ptr);
        const bool data_arrived = (*mshr_it)->isDataArrived();
        const bool is_store_inst = mem_access_info_ptr->getInstPtr()->isStoreInst();

        // All ST are considered Hit
        if (is_store_inst)
        {
            // Update Line fill buffer only if ST
            ILOG("Write to Line fill buffer (ST), block address:0x" << std::hex << block_addr);
            (*mshr_it)->setModified(true);
            (*mshr_it)->setMemRequest(mem_access_info_ptr);
            mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
            out_lsu_lookup_ack_.send(mem_access_info_ptr);
            return;
        }

        if (data_arrived)
        {
            ILOG("Hit on Line fill buffer (LD), block address:0x" << std::hex << block_addr);
            mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
            out_lsu_lookup_ack_.send(mem_access_info_ptr);
            return;
        }

        // Enqueue Load in LMQ
        ILOG("Load miss inst to LMQ; block address:0x" << std::hex << block_addr);
        (*mshr_it)->setMemRequest(mem_access_info_ptr);
        mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);
        out_lsu_lookup_req_.send(mem_access_info_ptr);
    }

    uint64_t DCache::getBlockAddr(const MemoryAccessInfoPtr & mem_access_info_ptr) const
    {
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        const auto & inst_target_addr = inst_ptr->getRAdr();
        return addr_decoder_->calcBlockAddr(inst_target_addr);
    }

    // Data read stage
    void DCache::handleDataRead_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::DATA_READ);
        const MemoryAccessInfoPtr & mem_access_info_ptr = cache_pipeline_[stage_id];
        ILOG(mem_access_info_ptr << " in read stage");
        if (mem_access_info_ptr->isRefill())
        {
            reloadCache_(mem_access_info_ptr->getPhyAddr());
            return;
        }

        if (mem_access_info_ptr->isCacheHit())
        {
            mem_access_info_ptr->setDataReady(true);
        }
        else
        {
            if (!l2cache_busy_)
            {
                out_l2cache_req_.send(mem_access_info_ptr);
                l2cache_busy_ = true;
            }
            else
            {
                uev_mshr_request_.schedule(sparta::Clock::Cycle(1));
            }
        }
        out_lsu_lookup_ack_.send(mem_access_info_ptr);
    }

    void DCache::mshrRequest_()
    {
        if (!l2cache_busy_)
        {
            auto iter = mshr_file_.begin();
            while (iter != mshr_file_.end())
            {

                if (iter.isValid())
                {
                    const auto & mshr_entry = *iter;
                    auto mem_info = mshr_entry->getMemRequest();
                    if (mshr_entry->isValid() && !mshr_entry->isDataArrived() && mem_info)
                    {
                        ILOG("Sending mshr request when not busy " << mem_info);
                        out_l2cache_req_.send(mem_info);
                        l2cache_busy_ = true;
                        break;
                    }
                }
                ++iter;
            }
        }
    }

    void DCache::handleDeallocate_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::DEALLOCATE);
        const MemoryAccessInfoPtr & mem_access_info_ptr = cache_pipeline_[stage_id];
        ILOG(mem_access_info_ptr << " in deallocate stage");
        if (mem_access_info_ptr->isRefill())
        {
            const auto & mshr_it = mem_access_info_ptr->getMSHRInfoIterator();
            if (mshr_it.isValid())
            {
                MemoryAccessInfoPtr dependant_load_inst = (*mshr_it)->getMemRequest();
                out_lsu_lookup_ack_.send(dependant_load_inst);

                ILOG("Removing mshr entry for " << mem_access_info_ptr);
                mshr_file_.erase(mem_access_info_ptr->getMSHRInfoIterator());
            }
            return;
        }
        ILOG("Deallocating pipeline for " << mem_access_info_ptr);
    }

    void DCache::receiveMemReqFromLSU_(const MemoryAccessInfoPtr & memory_access_info_ptr)
    {

        ILOG("Got memory access request from LSU " << memory_access_info_ptr);
        if (!cache_refill_selected_)
        {
            ILOG("Arbitration from refill " << memory_access_info_ptr);
            memory_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::RELOAD);
            out_lsu_lookup_ack_.send(memory_access_info_ptr);
            return;
        }
        cache_pipeline_.append(memory_access_info_ptr);
        out_lsu_lookup_ack_.send(memory_access_info_ptr);
        uev_free_pipeline_.schedule(1);

        uev_mshr_request_.schedule(1);
    }

    void DCache::receiveRespFromL2Cache_(const MemoryAccessInfoPtr & memory_access_info_ptr)
    {
        ILOG("Received cache refill " << memory_access_info_ptr);
        // We mark the mem access to refill, this could be moved to the lower level caches later
        memory_access_info_ptr->setIsRefill(true);
        l2cache_busy_ = false;
        cache_pipeline_.append(memory_access_info_ptr);
        cache_refill_selected_ = false;
        uev_free_pipeline_.schedule(1);

        uev_mshr_request_.schedule(1);
    }

    void DCache::receiveAckFromL2Cache_(const uint32_t & ack)
    {
        // When DCache sends the request to L2Cache for a miss,
        // This bool will be set to false, and Dcache should wait for ack from
        // L2Cache notifying DCache that there is space in it's dcache request buffer
        //
        // Set it to true so that the following misses from DCache can be sent out to L2Cache.
        dcache_l2cache_credits_ = ack;
    }

    void DCache::freePipelineAppend_()
    {
        ILOG("Pipeline is freed");
        cache_refill_selected_ = true;
    }

    // MSHR Entry allocation in case of miss
    void DCache::allocateMSHREntry_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        sparta_assert(mshr_file_.size() <= num_mshr_entries_, "Appending mshr causes overflows!");

        MSHREntryInfoPtr mshr_entry = sparta::allocate_sparta_shared_pointer<MSHREntryInfo>(
            mshr_entry_allocator_, cache_line_size_, getClock());

        const auto & it = mshr_file_.push_back(mshr_entry);
        mem_access_info_ptr->setMSHREntryInfoIterator(it);
    }

} // namespace olympia
