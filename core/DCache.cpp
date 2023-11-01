#include "DCache.hpp"
#include "LSU.hpp"
#include "MemoryAccessInfo.hpp"
#include <sparta/events/SchedulingPhases.hpp>

namespace olympia {
    const char DCache::name[] = "cache";

    DCache::DCache(sparta::TreeNode *node, const CacheParameterSet *p) :
            sparta::Unit(node),
            mshr_entry_allocator(50, 30),    // 50 and 30 are arbitrary numbers here.  It can be tuned to an exact value.
            mshr_file_("mshr_file",p->mshr_entries, getClock()),
            l1_always_hit_(p->l1_always_hit),
            cache_latency_(p->cache_latency),
            cache_line_size_(p->l1_line_size),
            max_mshr_entries_(p->mshr_entries),
            load_miss_queue_size_(p->load_miss_queue_size)
    {
        sparta_assert(p->mshr_entries > 0, "There must be atleast 1 MSHR entry");
        sparta_assert(p->load_miss_queue_size > 0, "There must be atleast 1 LMQ entry");

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
        cache_pipeline_.registerHandlerAtStage<sparta::SchedulingPhase::Update>
            (static_cast<uint32_t>(PipelineStage::LOOKUP),
             CREATE_SPARTA_HANDLER(DCache, lookupHandler_));

        cache_pipeline_.registerHandlerAtStage<sparta::SchedulingPhase::Update>
            (static_cast<uint32_t>(PipelineStage::DATA_READ),
             CREATE_SPARTA_HANDLER(DCache, dataHandler_));
    }

    DCache::~DCache() {
        DLOG(
            getContainer()->getLocation()
            << ": " << mshr_entry_allocator.getNumAllocated() << " MSHREntryInfo objects allocated/created"
        );
    }

    // L1 Cache lookup
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

    // MSHR File Lookup
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

    // MSHR Entry allocation in case of miss
    sparta::Buffer<DCache::MSHREntryInfoPtr>::iterator DCache::allocateMSHREntry_(uint64_t block_address){

        MSHREntryInfoPtr mshr_entry = sparta::allocate_sparta_shared_pointer<MSHREntryInfo>(mshr_entry_allocator,
                                                                                            block_address, 
                                                                                            cache_line_size_, 
                                                                                            load_miss_queue_size_,
                                                                                            getClock());
        auto it = mshr_file_.push_back(mshr_entry);
        return it;
    }
    
    // Handle request from the LSU
    void DCache::processInstsFromLSU_(const MemoryAccessInfoPtr &memory_access_info_ptr){
        // Check if there is an incoming cache refill request from BIU
        // ILOG("Incoming Instruction from LSU " << memory_access_info_ptr);

        if(incoming_cache_refill_) {
            // Cache refill is given priority
            // nack signal is sent to the LSU
            // out_lsu_lookup_nack_.send(1);
            // Temporary: send miss instead of nack
            memory_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);
            out_lsu_lookup_resp_.send(memory_access_info_ptr,1);

            ILOG("Unable to handle LSU request" << memory_access_info_ptr << "; Incoming Cache refill");
        }
        else {
            // Append to Cache Pipeline
            ILOG("Cache request appended to pipeline " << memory_access_info_ptr);
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
        // TODO keep a pointer of ongoing cache refill
        ILOG("Incoming Refill from BIU");
        // temporary event
        incoming_cache_refill_ = true;
        
        // Write cache line to line fill buffer of ongoing mshr entry refill request
        (*current_refill_mshr_entry_)->setDataArrived(true);

        // Wake up all dependant loads
        MemoryAccessInfoPtr dependant_load_inst = (*current_refill_mshr_entry_)->dequeueLoad();
        while(dependant_load_inst != nullptr) {
            out_lsu_data_ready_.send(dependant_load_inst);
            dependant_load_inst = (*current_refill_mshr_entry_)->dequeueLoad();
        }
        ILOG("Waking up dependant load instructions on block address:0x" << std::hex << (*current_refill_mshr_entry_)->getBlockAddress());

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

        ILOG("Cache Lookup Stage, " << mem_access_info_ptr);

        // Check if instruction is a store
        const bool is_store_inst = inst_ptr->isStoreInst();
        
        const bool hit_on_cache = cacheLookup_(mem_access_info_ptr);

        if(hit_on_cache){
            mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
            // Send Lookup Ack to LSU and proceed to next stage
            ILOG("Send Lookup "<< mem_access_info_ptr->getCacheState() << " to LSU");
            out_lsu_lookup_resp_.send(mem_access_info_ptr);
        }
        else{
            // Check MSHR Entries for address match
            auto mshr_idx = mshrLookup_(block_addr);

            // if address matches
            if(mshr_idx != mshr_file_.end()){
                ILOG("MSHR Entry Exists, block address:0x" << std::hex << block_addr);

                mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);
                bool data_arrived = (*mshr_idx)->getDataArrived();

                // All ST are considered Hit
                if(is_store_inst) {
                    // Update Line fill buffer only if ST
                    ILOG("Write to Line fill buffer (ST), block address:0x" << std::hex << block_addr);
                    (*mshr_idx)->setModified(true);
                    mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
                } else {
                    if (data_arrived) {
                        ILOG("Hit on Line fill buffer (LD), block address:0x" << std::hex << block_addr);
                        mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
                    } else {
                        // Check if LMQ is full
                        if((*mshr_idx)->isLoadMissQueueFull()) {
                            ILOG("Load miss and LMQ full (nack); block address:0x" << std::hex << block_addr);
                            // nack
                            // Temporary: send miss instead of nack
                            mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);
                            out_lsu_lookup_resp_.send(mem_access_info_ptr);
                            return;
                        }

                        ILOG("Load miss and enqueue load instruction to LMQ; block address:0x" << std::hex << block_addr);
                        // Enqueue Load in LMQ
                        (*mshr_idx)->enqueueLoad(mem_access_info_ptr);
                    }
                }

                ILOG("Send Lookup "<< mem_access_info_ptr->getCacheState() << " to LSU"); // Send Lookup Ack to LSU and proceed to next stage
                out_lsu_lookup_resp_.send(mem_access_info_ptr);

            }
            else {
                // Check if MSHR entry available
                if(mshr_file_.numFree() > 0) {
                    ILOG("Allocate MSHR Entry, block address:0x" << std::hex << block_addr);
                    auto mshr_idx = allocateMSHREntry_(block_addr);
                    mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);

                    if(is_store_inst) {
                        ILOG("Write to Line fill buffer (ST), block address:0x" << std::hex << block_addr);
                        // Update Line fill buffer
                        (*mshr_idx)->setModified(true);
                        mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
                    } 
                    else {
                        ILOG("Load miss and enqueue load inst to LMQ; block address:0x" << std::hex << block_addr);
                        // Enqueue Load in LMQ
                        (*mshr_idx)->enqueueLoad(mem_access_info_ptr);
                    }
                    // Send Lookup Ack to LSU and proceed to next stage
                    ILOG("Send Lookup "<< mem_access_info_ptr->getCacheState() << " to LSU"); // Send Lookup Ack to LSU and proceed to next stage
                    out_lsu_lookup_resp_.send(mem_access_info_ptr);
                }
                else {
                    // Cache is unable to handle ld/st request
                    // out_lsu_lookup_nack_.send();

                    ILOG("No MSHR Entry available (nack), block_address:0x" << std::hex << block_addr);
                    // Temporary: send miss instead of nack
                    mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);
                    out_lsu_lookup_resp_.send(mem_access_info_ptr);
                }
            }
        }
    }

    // Cache Pipeline Data Read Stage
    void DCache::dataHandler_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::DATA_READ);
        const MemoryAccessInfoPtr & mem_access_info_ptr = cache_pipeline_[stage_id];
        ILOG("Cache Data Stage " << mem_access_info_ptr);

        if  (mem_access_info_ptr->isCacheHit()) {
            ILOG("Set DataReady to true, uid: " << mem_access_info_ptr->getInstUniqueID());
            mem_access_info_ptr->setDataReady(true);
        }
        else {
            ILOG("Issue Downstream read");
            // Initiate read to downstream
            uev_issue_downstream_read_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Send cache refill request
    void DCache::issueDownstreamRead_() {
        // If there is no ongoing refill request
        // Issue MSHR Refill request to downstream
        if(ongoing_cache_refill_) {
            ILOG("Cannot issue downstream read; ongoing cache refill")
            return;
        }

        // Requests are sent in FIFO order
        current_refill_mshr_entry_ = mshr_file_.begin();

        if(current_refill_mshr_entry_ == mshr_file_.end()){
            ILOG("No MSHR entry to be issued");
            return;
        }

        // Only 1 ongoing refill request at any time
        ongoing_cache_refill_ = true;

        // Send BIU request
        // out_biu_req_.send(cache_line);
        // (Temporary) call event after fixed time (4 cycles)
        uev_biu_incoming_refill_.schedule(4);
        ILOG("Downstream read issued");
    }

    // Line Fill buffer written to cache
    void DCache::cacheRefillWrite_() {
        incoming_cache_refill_ = false;
        // Write line buffer into cache
        // Initiate write to downstream in case victim line TODO
        auto block_address = (*current_refill_mshr_entry_)->getBlockAddress();
        auto l1_cache_line = &l1_cache_->getLineForReplacementWithInvalidCheck(block_address);

        // Write line buffer data onto cache line
        l1_cache_line->reset(block_address);
        l1_cache_line->setModified((*current_refill_mshr_entry_)->isModified());
        l1_cache_->touchMRU(*l1_cache_line);

        ILOG("Deallocate MSHR Entry, block address:0x" << std::hex << block_address);
        // Deallocate MSHR entry (after a cycle)
        mshr_file_.erase(current_refill_mshr_entry_);
        ongoing_cache_refill_ = false;
        // reissue downstream read (after a cycle)
        uev_issue_downstream_read_.schedule(1);

    }
}
