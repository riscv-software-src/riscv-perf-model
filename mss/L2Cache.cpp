// <L2Cache.cpp> -*- C++ -*-

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/LogUtils.hpp"

#include "L2Cache.hpp"

namespace olympia_mss
{
    const char L2Cache::name[] = "l2cache";

    ////////////////////////////////////////////////////////////////////////////////
    // Constructor
    ////////////////////////////////////////////////////////////////////////////////

    L2Cache::L2Cache(sparta::TreeNode *node, const L2CacheParameterSet *p) :
        sparta::Unit(node),
        num_reqs_from_dcache_(&unit_stat_set_,
                     "num_reqs_from_dcache",
                     "The total number of instructions received by L2Cache from DCache",
                     sparta::Counter::COUNT_NORMAL),
        num_reqs_from_il1_(&unit_stat_set_,
                     "num_reqs_from_il1",
                     "The total number of instructions received by L2Cache from IL1",
                     sparta::Counter::COUNT_NORMAL),
        num_reqs_to_biu_(&unit_stat_set_,
                     "num_reqs_to_biu",
                     "The total number of instructions forwarded from L2Cache to BIU",
                     sparta::Counter::COUNT_NORMAL),
        num_acks_from_biu_(&unit_stat_set_,
                     "num_acks_from_biu",
                     "The total number of instructions received from BIU into L2Cache",
                     sparta::Counter::COUNT_NORMAL),
        num_acks_to_il1_(&unit_stat_set_,
                     "num_acks_to_il1",
                     "The total number of instructions forwarded from L2Cache to IL1",
                     sparta::Counter::COUNT_NORMAL),
        num_acks_to_dcache_(&unit_stat_set_,
                     "num_acks_to_dcache",
                     "The total number of instructions forwarded from L2Cache to DCache",
                     sparta::Counter::COUNT_NORMAL),
        num_resps_from_biu_(&unit_stat_set_,
                     "num_resps_from_biu",
                     "The total number of instructions received from BIU into L2Cache",
                     sparta::Counter::COUNT_NORMAL),
        num_resps_to_il1_(&unit_stat_set_,
                     "num_resps_to_il1",
                     "The total number of instructions forwarded from L2Cache to IL1",
                     sparta::Counter::COUNT_NORMAL),
        num_resps_to_dcache_(&unit_stat_set_,
                     "num_resps_to_dcache",
                     "The total number of instructions forwarded from L2Cache to DCache",
                     sparta::Counter::COUNT_NORMAL),
        l2_cache_hits_(&unit_stat_set_,
                     "l2_cache_hits",
                     "The total number L2 Cache Hits",
                     sparta::Counter::COUNT_NORMAL),
        l2_cache_misses_(&unit_stat_set_,
                     "l2_cache_misses",
                     "The total number L2 Cache Misses",
                     sparta::Counter::COUNT_NORMAL),
        dcache_req_queue_size_(p->dcache_req_queue_size),
        il1_req_queue_size_(p->il1_req_queue_size),
        biu_req_queue_size_(p->biu_req_queue_size),
        biu_resp_queue_size_(p->biu_resp_queue_size),
        dcache_resp_queue_size_(p->dcache_resp_queue_size),
        il1_resp_queue_size_(p->il1_resp_queue_size),
        stages_(p->l2cache_latency),
        l2cache_pipeline_("L2CachePipeline", stages_.NUM_STAGES, getClock()),
        pipeline_req_queue_("Pipeline_Request_Queue",
                            p->pipeline_req_queue_size,
                            node->getClock()),
        pipeline_req_queue_size_(p->pipeline_req_queue_size),
        miss_pending_buffer_("Miss_Pending_Buffer", 
                                p->miss_pending_buffer_size, 
                                node->getClock(), 
                                &unit_stat_set_),
        miss_pending_buffer_size_(p->miss_pending_buffer_size),
        l2_always_hit_(p->l2_always_hit),
        l2cache_biu_credits_(p->l2cache_biu_credits),
        l2cache_latency_(p->l2cache_latency) {

    	// In Port Handler registration
        in_dcache_l2cache_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L2Cache, getReqFromDCache_, olympia::InstPtr));

        in_il1_l2cache_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L2Cache, getReqFromIL1_, olympia::InstPtr));

        in_biu_resp_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L2Cache, getRespFromBIU_, olympia::InstPtr));

        in_biu_ack_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L2Cache, getAckFromBIU_, bool));

        // Default channel selected for arbitration
        channel_select_ = channel::IL1;

        // Pipeline collection config
        l2cache_pipeline_.enableCollection(node);
        // Allow the pipeline to create events and schedule work
        l2cache_pipeline_.performOwnUpdates();

        // There can be situations where NOTHING is going on in the
        // simulator but forward progression of the pipeline elements.
        // In this case, the internal event for the pipeline will
        // be the only event keeping simulation alive.  Sparta
        // supports identifying non-essential events (by calling
        // setContinuing to false on any event).
        l2cache_pipeline_.setContinuing(true);

        l2cache_pipeline_.registerHandlerAtStage(stages_.CACHE_LOOKUP, CREATE_SPARTA_HANDLER(L2Cache, handleCacheAccessRequest_));

        l2cache_pipeline_.registerHandlerAtStage(stages_.HIT_MISS_HANDLING, CREATE_SPARTA_HANDLER(L2Cache, handleCacheAccessResult_));

        // L2 cache config
        l2_lineSize_ = p->l2_line_size;
        shiftBy_ = log2(l2_lineSize_);
        const uint32_t l2_size_kb = p->l2_size_kb;
        const uint32_t l2_associativity = p->l2_associativity;
        std::unique_ptr<sparta::cache::ReplacementIF> repl(new sparta::cache::TreePLRUReplacement
                                                         (l2_associativity));
        l2_cache_.reset(new olympia::SimpleDL1( getContainer(), l2_size_kb, l2_lineSize_, *repl));
        
        ILOG("L2Cache construct: #" << node->getGroupIdx());
        ILOG("Starting BIU credits = " << l2cache_biu_credits_);
    }


    ////////////////////////////////////////////////////////////////////////////////
    // Callbacks
    ////////////////////////////////////////////////////////////////////////////////

    // Receive new L2Cache request from DCache
    void L2Cache::getReqFromDCache_(const olympia::InstPtr & inst_ptr) {
        
        ILOG("Request received from DCache on the port");

        appendDCacheReqQueue_(inst_ptr);

        // Schedule L2Cache request handling event only when:
        // Request queue is not empty
        if (dcache_req_queue_.size() < dcache_req_queue_size_) {
            ev_handle_dcache_l2cache_req_.schedule(sparta::Clock::Cycle(0));
            num_reqs_from_dcache_++;
        }
        else {
            ILOG("This request cannot be serviced right now, L2Cache input buffer from DCache is already full!");
        }
    }

    // Receive new L2Cache request from IL1
    void L2Cache::getReqFromIL1_(const olympia::InstPtr & inst_ptr) {
        
        ILOG("Request received from IL1 on the port");

        appendIL1ReqQueue_(inst_ptr);

        // Schedule L2Cache request handling event only when:
        // (1)Request queue is not empty
        if (il1_req_queue_.size() < il1_req_queue_size_) {
            ev_handle_il1_l2cache_req_.schedule(sparta::Clock::Cycle(0));
            num_reqs_from_il1_++;
        }
        else {
            ILOG("This request cannot be serviced right now, L2Cache input buffer from IL1 is already full!");
        }
    }

    // Handle BIU resp
    void L2Cache::getRespFromBIU_(const olympia::InstPtr & inst_ptr) {
        
        ILOG("Response received from BIU on the port");
        
        appendBIURespQueue_(inst_ptr);

        // Schedule BIU resp handling event only when:
        // Request queue is not empty
        if (biu_resp_queue_.size() < biu_resp_queue_size_) {
            ev_handle_biu_l2cache_resp_.schedule(sparta::Clock::Cycle(0));
            num_resps_from_biu_++;
        }
        else {
            ILOG("This request cannot be serviced right now, L2Cache input buffer from DCache is already full!");
        }
    }

    // Handle BIU ack
    void L2Cache::getAckFromBIU_(const bool & ack) {

        // Update the biu credits
        l2cache_biu_credits_++;

        // Kickstart the pipeline issueing 
        ev_issue_req_.schedule(sparta::Clock::Cycle(0));

        ILOG("Ack received from BIU on the port : Current BIU credits = " << l2cache_biu_credits_);
    }

    // Handle L2Cache request from DCache
    void L2Cache::handle_DCache_L2Cache_Req_() {
        if (!dcache_req_queue_.empty()) {
            ev_create_req_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Handle L2Cache request from IL1
    void L2Cache::handle_IL1_L2Cache_Req_() {
        if (!il1_req_queue_.empty())  {
            ev_create_req_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Handle BIU->L2Cache response
    void L2Cache::handle_BIU_L2Cache_Resp_() {
        if (!biu_resp_queue_.empty())  {
            ev_create_req_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Handle L2Cache request to BIU
    void L2Cache::handle_L2Cache_BIU_Req_() {
        out_biu_req_.send(biu_req_queue_.front());
        biu_req_queue_.erase(biu_req_queue_.begin());

        num_reqs_to_biu_++;

        ILOG("L2Cache Request sent to BIU!");
    }

    // Returning ack to DCache
    void L2Cache::handle_L2Cache_DCache_Ack_() {
        out_l2cache_dcache_ack_.send(true);
        num_acks_to_dcache_++;

        ILOG("L2Cache Ack is sent to DCache!");
    }

    // Returning resp to IL1
    void L2Cache::handle_L2Cache_IL1_Ack_() {
        out_l2cache_il1_ack_.send(true);
        num_acks_to_il1_++;
        
        ILOG("L2Cache Ack is sent to IL1!");
    }      

    // Returning resp to DCache
    void L2Cache::handle_L2Cache_DCache_Resp_() {
        out_l2cache_dcache_resp_.send(dcache_resp_queue_.front());
        dcache_resp_queue_.erase(dcache_resp_queue_.begin());
        
        num_resps_to_dcache_++;

        ILOG("L2Cache Resp is sent to DCache!");
    }

    // Returning resp to IL1
    void L2Cache::handle_L2Cache_IL1_Resp_() {
        out_l2cache_il1_resp_.send(il1_resp_queue_.front());
        il1_resp_queue_.erase(il1_resp_queue_.begin());

        num_resps_to_il1_++;

        ILOG("L2Cache Resp is sent to IL1!");
    }

    // Handle arbitration and forward the req to pipeline_req_queue_
    void L2Cache::create_Req_() {

        channel arbitration_winner = arbitrateL2CacheAccessReqs_();
        uint32_t num_inst_found = 0;

        if (arbitration_winner == channel::BIU) {
            
            const olympia::InstPtr &instPtr = biu_resp_queue_.front();
            
            // request in the miss_pending_buffer iterator
            auto iter = miss_pending_buffer_.begin();
            auto req = iter;
            
            // Function to check if the request to the given cacheline is present in the miss_pending_buffer_ 
            auto getCacheLine = [this] (auto inst_ptr) { return inst_ptr->getRAdr() >> shiftBy_; }; 
            auto is_addr_present = [instPtr, getCacheLine] (auto reqPtr)
                        { return getCacheLine(reqPtr->getInstPtr()) == getCacheLine(instPtr); };

            // Find the instructions from the miss_pending_buffer_
            while (iter != miss_pending_buffer_.end()) {

                req = std::find_if(iter, miss_pending_buffer_.end(), is_addr_present);
                iter = req;

                // Set the original SrcUnit as the DestUnit because the resp will 
                // now be forwarded from BIU to the original SrcUnit          
                if (req != miss_pending_buffer_.end()) {

                    ILOG("Request found in miss_pending_buffer_ with SrcUnit : " << (*req)->getSrcUnit());
                    num_inst_found++;

                    (*req)->setDestUnit((*req)->getSrcUnit());
                    (*req)->setSrcUnit(L2UnitName::BIU);

                    if (pipeline_req_queue_.size() < pipeline_req_queue_size_) {
                        pipeline_req_queue_.push(*req);
                    }
                    else {
                        sparta_assert("pipeline_req_queue_ is full. Check the sizing.")
                    }

                    if (num_inst_found == 1)
                        ILOG("BIU cache reload request is sent to Pipeline_req_Q!");

                    iter++;
                    
                    miss_pending_buffer_.erase(req);
                }
            }
            
            if (num_inst_found == 0)
                sparta_assert("No match found for the BIU resp in the miss_pending_buffer!!");

            biu_resp_queue_.erase(biu_resp_queue_.begin());
        }
        else if (arbitration_winner == channel::IL1) {
            
            const auto &reqPtr = std::make_shared<olympia::MemoryAccessInfo>(il1_req_queue_.front());
            
            reqPtr->setSrcUnit(L2UnitName::IL1);

            pipeline_req_queue_.push(reqPtr);
            ILOG("IL1 request is sent to Pipeline_req_Q!");

            il1_req_queue_.erase(il1_req_queue_.begin());
            
            // Send out the ack to IL1 for credit management
            ev_handle_l2cache_il1_ack_.schedule(sparta::Clock::Cycle(0));
        }
        else if (arbitration_winner == channel::DCACHE) {
            
            const auto &reqPtr = std::make_shared<olympia::MemoryAccessInfo>(dcache_req_queue_.front());
            
            reqPtr->setSrcUnit(L2UnitName::DCACHE);

            pipeline_req_queue_.push(reqPtr);
            ILOG("DCache request is sent to Pipeline_req_Q!");

            dcache_req_queue_.erase(dcache_req_queue_.begin());
            
            // Send out the ack to DCache for credit management
            ev_handle_l2cache_dcache_ack_.schedule(sparta::Clock::Cycle(0));
        }

        ev_issue_req_.schedule(sparta::Clock::Cycle(0));

        if (   (!biu_resp_queue_.empty() 
            ||  !il1_req_queue_.empty()
            ||  !dcache_req_queue_.empty() )

            && (pipeline_req_queue_.numFree() > 0))
            
            ev_create_req_.schedule(sparta::Clock::Cycle(1));
    }

    void L2Cache::issue_Req_() {
     
        // Append the request to a pipeline if the pipeline_req_queue_ is not empty
        // and l2cache_pipeline_ has credits available
        if (hasCreditsForPipelineIssue_() && !pipeline_req_queue_.empty()) {
            l2cache_pipeline_.append(pipeline_req_queue_.front());
            inFlight_reqs_++;
            ILOG("Request is sent to Pipeline! SrcUnit : " << pipeline_req_queue_.front()->getSrcUnit());

            pipeline_req_queue_.pop(); 
        }

        // Checking for the queue empty again before scheduling the event for the next clock cycle
        if (!pipeline_req_queue_.empty())
            ev_issue_req_.schedule(sparta::Clock::Cycle(1));
    }

    // Pipeline Stage 1
    void L2Cache::handleCacheAccessRequest_() {
        const auto req = l2cache_pipeline_[stages_.CACHE_LOOKUP];
        ILOG("Pipeline stage 1! - " << req->getInstPtr());
        
        // Access cache, and check cache hit or miss
        const L2CacheState cacheLookUpResult = cacheLookup_(req);
        
        // Update memory access info
        req->setCacheState(cacheLookUpResult);
        
        if (cacheLookUpResult == L2CacheState::MISS) { 
            if (req->getSrcUnit() == L2UnitName::BIU) {
            
                // Reload cache line
                reloadCache_(req->getInstPtr()->getRAdr());
                req->setCacheState(L2CacheState::HIT);
                ILOG("Post-Reload State set to : Cache HIT: phyAddr=0x" << std::hex << req->getInstPtr()->getRAdr());
            }
        }
        else if (cacheLookUpResult == L2CacheState::HIT) {
            if (req->getSrcUnit() != L2UnitName::BIU) {

                // Return the request back to where it came from
                req->setDestUnit(req->getSrcUnit());
            }
        }
    }

    // Pipeline Stage 2
    void L2Cache::handleCacheAccessResult_() {
        const auto req = l2cache_pipeline_[stages_.HIT_MISS_HANDLING];
        ILOG("Pipeline stage 2! " << req->getInstPtr());
        
        inFlight_reqs_--;

        if (req->getCacheState() == L2CacheState::HIT) {
            // If it was originally a miss in L2Cache, on return from BIU, it's SrcUnit is set to BIU
            // and DestUnit to whatever the original SrcUnit was.
            //
            // If it was a hit in L2Cache, return the request back to where it originally came from.
            // 
            // Send out the resp to the original SrcUnit -- which is now the DestUnit. 
            sendOutResp_(req->getDestUnit(), req->getInstPtr());
        }
        else { // if (req->getCacheState() == L2CacheState::MISS)
            // This request to access cache came from DCache or IL1 to do a cache lookup.
            // It was either a miss or hit based on cacheLookup_() in the previous stage of the pipeline
            
                
            // Set Destination for this request to BIU
            req->setDestUnit(L2UnitName::BIU);

            // Handle the miss instruction by storing it aside while waiting 
            // for lower level memory to return
            if (miss_pending_buffer_.size() < miss_pending_buffer_size_) {
                ILOG("Storing the CACHE MISS in miss_pending_buffer_");
                miss_pending_buffer_.push_back(req);
            }
            else {
                sparta_assert("No space in miss_pending_buffer_!");
            }

            // Function to check if the request to the given cacheline is present in the miss_pending_buffer_ 
            auto getCacheLine = [this] (auto reqPtr) { return reqPtr->getInstPtr()->getRAdr() >> shiftBy_; };
            auto is_addr_present = [req, getCacheLine] (auto reqPtr)
                            { return (req != reqPtr && getCacheLine(reqPtr) == getCacheLine(req));};

            // Send out the request to BIU for a cache MISS if it is not sent out already
            auto reqIter = std::find_if(miss_pending_buffer_.begin(), miss_pending_buffer_.end(), is_addr_present);
                
            if (reqIter == miss_pending_buffer_.end())
                sendOutReq_(req->getDestUnit(), req->getInstPtr());
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Regular Function/Subroutine Call
    ////////////////////////////////////////////////////////////////////////////////

    // Append L2Cache request queue for reqs from DCache
    void L2Cache::appendDCacheReqQueue_(const olympia::InstPtr& inst_ptr) {
        sparta_assert(dcache_req_queue_.size() <= dcache_req_queue_size_ ,"DCache request queue overflows!");

        // Push new requests from back
        dcache_req_queue_.emplace_back(inst_ptr);

        ILOG("Append DCache->L2Cache request queue!");
    }

    // Append L2Cache request queue for reqs from IL1
    void L2Cache::appendIL1ReqQueue_(const olympia::InstPtr& inst_ptr) {
        sparta_assert(il1_req_queue_.size() <= il1_req_queue_size_ ,"IL1 request queue overflows!");

        // Push new requests from back
        il1_req_queue_.emplace_back(inst_ptr);

        ILOG("Append IL1->L2Cache request queue!");
    }

    // Append BIU resp queue
    void L2Cache::appendBIURespQueue_(const olympia::InstPtr& inst_ptr) {
        sparta_assert(biu_resp_queue_.size() <= biu_resp_queue_size_ ,"BIU resp queue overflows!");

        // Push new requests from back
        biu_resp_queue_.emplace_back(inst_ptr);

        ILOG("Append BIU->L2Cache resp queue!");
    }

    // Append DCache resp queue
    void L2Cache::appendDCacheRespQueue_(const olympia::InstPtr& inst_ptr) {
        sparta_assert(dcache_resp_queue_.size() <= dcache_resp_queue_size_ ,"DCache resp queue overflows!");
        
        // Push new resp to the dcache_resp_queue_
        dcache_resp_queue_.emplace_back(inst_ptr);
        ev_handle_l2cache_dcache_resp_.schedule(sparta::Clock::Cycle(0));
        
        ILOG("Append L2Cache->DCache resp queue!");
    }

    // Append IL1 resp queue
    void L2Cache::appendIL1RespQueue_(const olympia::InstPtr& inst_ptr) {
        sparta_assert(il1_resp_queue_.size() <= il1_resp_queue_size_ ,"IL1 resp queue overflows!");
        
        // Push new resp to the il1_resp_queue_
        il1_resp_queue_.emplace_back(inst_ptr);
        ev_handle_l2cache_il1_resp_.schedule(sparta::Clock::Cycle(0));
        
        ILOG("Append L2Cache->IL1 resp queue!");
    }

    // Append BIU req queue
    void L2Cache::appendBIUReqQueue_(const olympia::InstPtr& inst_ptr) {
        sparta_assert(biu_req_queue_.size() <= biu_req_queue_size_ ,"BIU req queue overflows!");
        
        // Push new request to the biu_req_queue_ if biu credits are available with the L2Cache
        if (l2cache_biu_credits_ > 0) {
            biu_req_queue_.emplace_back(inst_ptr);
            l2cache_biu_credits_-- ;

            ev_handle_l2cache_biu_req_.schedule(sparta::Clock::Cycle(0));
        }
        else {
            sparta_assert("Can't send out this biu request :: OUT OF BIU CREDITS");
        }
        
        ILOG("Append L2Cache->BIU req queue : Remaining BIU credits = " << l2cache_biu_credits_);
    }
    
    // Return the resp to the master units
    void L2Cache::sendOutResp_(const L2UnitName &unit, const olympia::InstPtr& instPtr) {
        // if (instPtr is originally from DCache)
        if (unit == L2UnitName::DCACHE) {
            appendDCacheRespQueue_(instPtr);
        }
        // if (instPtr is originally from IL1)
        else if (unit == L2UnitName::IL1) {
            appendIL1RespQueue_(instPtr);
        }
        else {
            sparta_assert("Resp is being sent to a Unit that is not valid");
        }
    }

    // Send the request to the slave units
    void L2Cache::sendOutReq_(const L2UnitName &unit, const olympia::InstPtr& instPtr) {
        // if (instPtr is destined for BIU on L2Cache miss) 
        if (unit == L2UnitName::BIU) {
            appendBIUReqQueue_(instPtr);
        }
        else {
            sparta_assert("Request is being sent to a Unit that is not valid");
        }
    }

    // Select the channel to pick the request from
    // Current options : 
    //       BIU - P0
    //       DCache - P1 - RoundRobin Candidate
    //       DL1 - P1 - RoundRobin Candidate
    L2Cache::channel L2Cache::arbitrateL2CacheAccessReqs_() {
        sparta_assert(il1_req_queue_.size() > 0 || dcache_req_queue_.size() > 0 || biu_resp_queue_.size() > 0,
                                "Arbitration failed: Reqest queues are empty!");
        
        channel winner;

        // P0 priority to sevice the pending request in the buffer
        if (!biu_resp_queue_.empty()) {
            winner = channel::BIU;
            ILOG("Arbitration winner - BIU");

            return winner;
        }
        
        // RoundRobin for P1 Priority
        while (true) {
            
            if (channel_select_ == channel::IL1) {
                channel_select_ = channel::DCACHE;
                if (!il1_req_queue_.empty()) {
                    winner = channel::IL1;
                    ILOG("Arbitration winner - IL1");
                    
                    return winner;
                }
            }
            else if (channel_select_ == channel::DCACHE) {
                channel_select_ = channel::IL1;
                if (!dcache_req_queue_.empty()) {
                    winner = channel::DCACHE;
                    ILOG("Arbitration winner - DCache");

                    return winner;
                }
            }
        }
    }

    // Cache lookup for a HIT or MISS on a given request
    L2Cache::L2CacheState L2Cache::cacheLookup_(L2MemoryAccessInfoPtr mem_access_info_ptr) {
        const olympia::InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        uint64_t phyAddr = inst_ptr->getRAdr();

        bool cache_hit = false;

        if (l2_always_hit_) {
            cache_hit = true;
        }
        else {
            auto cache_line = l2_cache_->peekLine(phyAddr);
            cache_hit = (cache_line != nullptr) && cache_line->isValid();

            // Update MRU replacement state if Cache HIT
            if (cache_hit) {
                l2_cache_->touchMRU(*cache_line);
            }
        }

        if (l2_always_hit_) {
            ILOG("HIT all the time: phyAddr=0x" << std::hex << phyAddr);
            l2_cache_hits_++;
        }
        else if (cache_hit) {
            ILOG("Cache HIT: phyAddr=0x" << std::hex << phyAddr);
            l2_cache_hits_++;
        }
        else {
            ILOG("Cache MISS: phyAddr=0x" << std::hex << phyAddr);
            l2_cache_misses_++;
        }

        return (cache_hit ? 
                      (L2CacheState::HIT) 
                    : (L2CacheState::MISS));
    }
    
    // Allocating the cacheline in the L2 based on return from BIU/L3
    void L2Cache::reloadCache_(uint64_t phyAddr) {
        auto l2_cache_line = &l2_cache_->getLineForReplacementWithInvalidCheck(phyAddr);
        l2_cache_->allocateWithMRUUpdate(*l2_cache_line, phyAddr);

        ILOG("Cache reload complete!");
    }

    // Check if there are enough credits for the request to be issued to the l2cache_pipeline_
    bool L2Cache::hasCreditsForPipelineIssue_() {

        uint32_t num_free_biu_req_queue = biu_req_queue_size_ - biu_req_queue_.size();
        uint32_t num_free_miss_pending_buffer_ = miss_pending_buffer_.numFree();

        uint32_t empty_slots = std::min(num_free_biu_req_queue, num_free_miss_pending_buffer_);

        DLOG("Inflight req : " << inFlight_reqs_ << " - Empty slots : " << empty_slots);
        return (inFlight_reqs_ < empty_slots);
    }
}
