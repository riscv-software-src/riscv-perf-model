// <L2Cache.cpp> -*- C++ -*-

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/LogUtils.hpp"

#include "L2Cache.hpp"

#include "OlympiaAllocators.hpp"

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
        num_reqs_from_icache_(&unit_stat_set_,
                     "num_reqs_from_icache",
                     "The total number of instructions received by L2Cache from ICache",
                     sparta::Counter::COUNT_NORMAL),
        num_reqs_to_biu_(&unit_stat_set_,
                     "num_reqs_to_biu",
                     "The total number of instructions forwarded from L2Cache to BIU",
                     sparta::Counter::COUNT_NORMAL),
        num_acks_from_biu_(&unit_stat_set_,
                     "num_acks_from_biu",
                     "The total number of instructions received from BIU into L2Cache",
                     sparta::Counter::COUNT_NORMAL),
        num_acks_to_icache_(&unit_stat_set_,
                     "num_acks_to_icache",
                     "The total number of instructions forwarded from L2Cache to ICache",
                     sparta::Counter::COUNT_NORMAL),
        num_acks_to_dcache_(&unit_stat_set_,
                     "num_acks_to_dcache",
                     "The total number of instructions forwarded from L2Cache to DCache",
                     sparta::Counter::COUNT_NORMAL),
        num_resps_from_biu_(&unit_stat_set_,
                     "num_resps_from_biu",
                     "The total number of instructions received from BIU into L2Cache",
                     sparta::Counter::COUNT_NORMAL),
        num_resps_to_icache_(&unit_stat_set_,
                     "num_resps_to_icache",
                     "The total number of instructions forwarded from L2Cache to ICache",
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
        icache_req_queue_size_(p->icache_req_queue_size),
        biu_req_queue_size_(p->biu_req_queue_size),
        biu_resp_queue_size_(p->biu_resp_queue_size),
        dcache_resp_queue_size_(p->dcache_resp_queue_size),
        icache_resp_queue_size_(p->icache_resp_queue_size),
        stages_(p->l2cache_latency),
        l2cache_pipeline_("L2CachePipeline", stages_.NUM_STAGES, getClock()),
        pipeline_req_queue_("Pipeline_Request_Queue",
                            p->pipeline_req_queue_size,
                            node->getClock()),
        miss_pending_buffer_("Miss_Pending_Buffer",
                                p->miss_pending_buffer_size,
                                node->getClock(),
                                &unit_stat_set_),
        miss_pending_buffer_size_(p->miss_pending_buffer_size),
        l2_lineSize_(p->l2_line_size),
        shiftBy_(log2(l2_lineSize_)),
        l2_always_hit_(p->l2_always_hit),
        l2cache_latency_(p->l2cache_latency),
        is_icache_connected_(p->is_icache_connected),
        is_dcache_connected_(p->is_dcache_connected),
        memory_access_allocator_(sparta::notNull(olympia::OlympiaAllocators::getOlympiaAllocators(node))->
                                 memory_access_allocator) {

    	// In Port Handler registration
        in_dcache_l2cache_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L2Cache, getReqFromDCache_, olympia::InstPtr));

        in_icache_l2cache_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L2Cache, getReqFromICache_, olympia::InstPtr));

        in_biu_resp_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L2Cache, getRespFromBIU_, olympia::InstPtr));

        in_biu_ack_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L2Cache, getAckFromBIU_, uint32_t));

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
        const uint32_t l2_size_kb = p->l2_size_kb;
        const uint32_t l2_associativity = p->l2_associativity;
        std::unique_ptr<sparta::cache::ReplacementIF> repl(new sparta::cache::TreePLRUReplacement
                                                         (l2_associativity));
        l2_cache_.reset(new olympia::CacheFuncModel( getContainer(), l2_size_kb, l2_lineSize_, *repl));

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(L2Cache, sendInitialCredits_));
        ILOG("L2Cache construct: #" << node->getGroupIdx());
    }


    ////////////////////////////////////////////////////////////////////////////////
    // Callbacks
    ////////////////////////////////////////////////////////////////////////////////
    // Sending Initial credits to I/D-Cache
    void L2Cache::sendInitialCredits_() {
        if (is_icache_connected_) {
            out_l2cache_icache_ack_.send(icache_req_queue_size_);
            ILOG("Sending initial credits to ICache : " << icache_req_queue_size_);
        }

        if (is_dcache_connected_) {
            out_l2cache_dcache_ack_.send(dcache_req_queue_size_);
            ILOG("Sending initial credits to DCache : " << dcache_req_queue_size_);
        }
    }

    // Receive new L2Cache request from DCache
    void L2Cache::getReqFromDCache_(const olympia::InstPtr & inst_ptr) {

        ILOG("Request received from DCache on the port");

        appendDCacheReqQueue_(inst_ptr);

        ev_handle_dcache_l2cache_req_.schedule(sparta::Clock::Cycle(0));
        ++num_reqs_from_dcache_;
    }

    // Receive new L2Cache request from ICache
    void L2Cache::getReqFromICache_(const olympia::InstPtr & inst_ptr) {

        ILOG("Request received from ICache on the port");

        appendICacheReqQueue_(inst_ptr);

        ev_handle_icache_l2cache_req_.schedule(sparta::Clock::Cycle(0));
        ++num_reqs_from_icache_;
    }

    // Handle BIU resp
    void L2Cache::getRespFromBIU_(const olympia::InstPtr & inst_ptr) {

        ILOG("Response received from BIU on the port");

        appendBIURespQueue_(inst_ptr);

        // Schedule BIU resp handling event only when:
        // Request queue is not empty
        if (biu_resp_queue_.size() <= biu_resp_queue_size_) {
            ev_handle_biu_l2cache_resp_.schedule(sparta::Clock::Cycle(0));
            ++num_resps_from_biu_;
        }
        else {
            sparta_assert(false, "This request cannot be serviced right now, L2Cache input buffer from DCache is already full!");
        }
    }

    // Handle BIU ack
    void L2Cache::getAckFromBIU_(const uint32_t & ack) {

        // Update the biu credits
        l2cache_biu_credits_ = ack;

        // Kickstart the pipeline issueing
        ev_issue_req_.schedule(1);

        ILOG("Ack received from BIU on the port : Current BIU credit available = " << l2cache_biu_credits_);
    }

    // Handle L2Cache request from DCache
    void L2Cache::handle_DCache_L2Cache_Req_() {
        if (!dcache_req_queue_.empty()) {
            ev_create_req_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Handle L2Cache request from ICache
    void L2Cache::handle_ICache_L2Cache_Req_() {
        if (!icache_req_queue_.empty())  {
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

        if (l2cache_biu_credits_ > 0 && !biu_req_queue_.empty()) {
            out_biu_req_.send(biu_req_queue_.front());
            --l2cache_biu_credits_;

            biu_req_queue_.erase(biu_req_queue_.begin());

            ++num_reqs_to_biu_;

            ILOG("L2Cache Request sent to BIU : Current BIU credit available = " <<l2cache_biu_credits_);
        }
        else {
            // Loop on biu_req_queue_ if the requests are present
            ev_handle_l2cache_biu_req_.schedule(sparta::Clock::Cycle(1));
        }
    }

    // Returning ack to DCache
    void L2Cache::handle_L2Cache_DCache_Ack_() {
        uint32_t available_slots = dcache_req_queue_size_ - dcache_req_queue_.size();
        out_l2cache_dcache_ack_.send(available_slots);
        ++num_acks_to_dcache_;

        ILOG("L2Cache->DCache :  Ack is sent.");
    }

    // Returning resp to ICache
    void L2Cache::handle_L2Cache_ICache_Ack_() {
        uint32_t available_slots = icache_req_queue_size_ - icache_req_queue_.size();
        out_l2cache_icache_ack_.send(available_slots);
        ++num_acks_to_icache_;

        ILOG("L2Cache->ICache :  Ack is sent.");
    }

    // Returning resp to DCache
    void L2Cache::handle_L2Cache_DCache_Resp_() {
        out_l2cache_dcache_resp_.send(dcache_resp_queue_.front());
        dcache_resp_queue_.erase(dcache_resp_queue_.begin());

        ++num_resps_to_dcache_;

        ILOG("L2Cache Resp is sent to DCache!");
    }

    // Returning resp to ICache
    void L2Cache::handle_L2Cache_ICache_Resp_() {
        out_l2cache_icache_resp_.send(icache_resp_queue_.front());
        icache_resp_queue_.erase(icache_resp_queue_.begin());

        ++num_resps_to_icache_;

        ILOG("L2Cache Resp is sent to ICache!");
    }

    // Handle arbitration and forward the req to pipeline_req_queue_
    void L2Cache::create_Req_() {

        Channel arbitration_winner = arbitrateL2CacheAccessReqs_();

        if (arbitration_winner == Channel::BIU) {

            const olympia::InstPtr &instPtr = biu_resp_queue_.front();

            // Function to check if the request to the given cacheline is present in the miss_pending_buffer_
            auto getCacheLine = [this] (auto inst_ptr) { return inst_ptr->getRAdr() >> shiftBy_; };
            auto const inst_cl = getCacheLine(instPtr);

            auto is_cl_present = [inst_cl, getCacheLine] (auto reqPtr)
                        { return getCacheLine(reqPtr->getInstPtr()) == inst_cl; };

            auto req = std::find_if(miss_pending_buffer_.begin(), miss_pending_buffer_.end(), is_cl_present);

            // Set the original SrcUnit as the DestUnit because the resp will
            // now be forwarded from BIU to the original SrcUnit
            if (req != miss_pending_buffer_.end()) {

                ILOG("Request found in miss_pending_buffer_ with SrcUnit : " << (*req)->getSrcUnit());

                (*req)->setDestUnit((*req)->getSrcUnit());
                (*req)->setSrcUnit(L2ArchUnit::BIU);
                (*req)->setCacheState(L2CacheState::RELOAD);

                if (pipeline_req_queue_.numFree() > 0) {
                    pipeline_req_queue_.push(*req);
                }
                else {
                    sparta_assert("pipeline_req_queue_ is full. Check the sizing.")
                }

                // Check if this was the last occuring
                auto iter = req; ++iter;
                auto next_req = std::find_if(iter, miss_pending_buffer_.end(), is_cl_present);

                if (next_req == miss_pending_buffer_.end()) {

                    // When the find_if() returns .end(), free the entry in the biu_resp_queue_
                    biu_resp_queue_.erase(biu_resp_queue_.begin());
                }

                // Free the entry in the miss_pending_buffer_
                miss_pending_buffer_.erase(req);
            }
        }
        else if (arbitration_winner == Channel::ICACHE) {

            const auto &reqPtr = sparta::allocate_sparta_shared_pointer<olympia::MemoryAccessInfo>(memory_access_allocator_,
                                                                         icache_req_queue_.front());

            reqPtr->setSrcUnit(L2ArchUnit::ICACHE);
            reqPtr->setDestUnit(L2ArchUnit::ICACHE);

            pipeline_req_queue_.push(reqPtr);
            ILOG("ICache request is sent to Pipeline_req_Q!");

            icache_req_queue_.erase(icache_req_queue_.begin());

            // Send out the ack to ICache for credit management
            ev_handle_l2cache_icache_ack_.schedule(sparta::Clock::Cycle(1));
        }
        else if (arbitration_winner == Channel::DCACHE) {

            const auto &reqPtr = sparta::allocate_sparta_shared_pointer<olympia::MemoryAccessInfo>(memory_access_allocator_,
                                                                         dcache_req_queue_.front());

            reqPtr->setSrcUnit(L2ArchUnit::DCACHE);
            reqPtr->setDestUnit(L2ArchUnit::DCACHE);

            pipeline_req_queue_.push(reqPtr);
            ILOG("DCache request is sent to Pipeline_req_Q!");

            dcache_req_queue_.erase(dcache_req_queue_.begin());

            // Send out the ack to DCache for credit management
            ev_handle_l2cache_dcache_ack_.schedule(sparta::Clock::Cycle(1));
        }
        else if (arbitration_winner == Channel::NO_ACCESS) {
            // Schedule a ev_create_req_ event again to see if the the new request
            // from any of the requestors can be put into pipeline_req_queue_
            ev_create_req_.schedule(sparta::Clock::Cycle(1));
        }
        else {
            sparta_assert(false, "Invalid arbitration winner, What Channel is picked up?");
        }

        // Try to issue a request to l2cache_pipeline_
        ev_issue_req_.schedule(1);

        // Schedule a ev_create_req_ event again to see if the the new request
        // from any of the requestors can be put into pipeline_req_queue_
        if (   !biu_resp_queue_.empty()
            || !icache_req_queue_.empty()
            || !dcache_req_queue_.empty() ) {
            ev_create_req_.schedule(sparta::Clock::Cycle(1));
        }
    }

    void L2Cache::issue_Req_() {

        // Append the request to a pipeline if the pipeline_req_queue_ is not empty
        // and l2cache_pipeline_ has credits available
        if (hasCreditsForPipelineIssue_() && !pipeline_req_queue_.empty()) {
            l2cache_pipeline_.append(pipeline_req_queue_.front());
            ++inFlight_reqs_;
            ILOG("Request is sent to Pipeline! SrcUnit : " << pipeline_req_queue_.front()->getSrcUnit());

            pipeline_req_queue_.pop();
        }

        // Checking for the queue empty again before scheduling the event for the next clock cycle
        if (!pipeline_req_queue_.empty()) {
            ev_issue_req_.schedule(1);
        }
    }

    // Pipeline Stage CACHE_LOOKUP
    void L2Cache::handleCacheAccessRequest_() {
        const auto req = l2cache_pipeline_[stages_.CACHE_LOOKUP];
        ILOG("Pipeline stage CACHE_LOOKUP : " << req->getInstPtr());

        const L2CacheState cacheLookUpResult = cacheLookup_(req);

        // Access cache, and check cache hit or miss
        if (req->getCacheState() == L2CacheState::RELOAD) {

            if (cacheLookUpResult == L2CacheState::MISS) {

                // Reload cache line
                reloadCache_(req->getInstPtr()->getRAdr());

                ILOG("Reload Complete: phyAddr=0x" << std::hex << req->getInstPtr()->getRAdr());
            }

            req->setCacheState(L2CacheState::HIT);
        }
        else {

            // Update memory access info
            req->setCacheState(cacheLookUpResult);
        }
    }

    // Pipeline Stage HIT_MISS_HANDLING
    void L2Cache::handleCacheAccessResult_() {
        const auto req = l2cache_pipeline_[stages_.HIT_MISS_HANDLING];
        ILOG("Pipeline stage HIT_MISS_HANDLING : " << req->getInstPtr());

        --inFlight_reqs_;

        // This request to access cache came from DCache or ICache to do a cache lookup.
        // It was either a miss or hit based on cacheLookup_() in the previous stage of the pipeline
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

            // Set Destination for this request to BIU
            req->setDestUnit(L2ArchUnit::BIU);

            // Handle the miss instruction by storing it aside while waiting
            // for lower level memory to return
            if (miss_pending_buffer_.size() < miss_pending_buffer_size_) {

                ILOG("Storing the CACHE MISS in miss_pending_buffer_");
                miss_pending_buffer_.push_back(req);
            }
            else {
                sparta_assert(false, "No space in miss_pending_buffer_! Why did the frontend issue push the request onto l2cache_pipeline_?");
            }

            // Function to check if the request to the given cacheline is present in the miss_pending_buffer_
            auto getCacheLine = [this] (auto reqPtr) { return reqPtr->getInstPtr()->getRAdr() >> shiftBy_; };
            const auto req_cl = getCacheLine(req);

            auto is_cl_present = [&req, req_cl, getCacheLine] (auto reqPtr)
                            { return (req != reqPtr && getCacheLine(reqPtr) == req_cl); };

            // Send out the request to BIU for a cache MISS if it is not sent out already
            auto reqIter = std::find_if(miss_pending_buffer_.rbegin(), miss_pending_buffer_.rend(), is_cl_present);

            if (reqIter == miss_pending_buffer_.rend()) {
                sendOutReq_(req->getDestUnit(), req->getInstPtr());
            }
            else {
                // Found a request to same cacheLine.
                // Link the current request to the last pending request
                (*reqIter)->setNextReq(req);
            }
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

    // Append L2Cache request queue for reqs from ICache
    void L2Cache::appendICacheReqQueue_(const olympia::InstPtr& inst_ptr) {
        sparta_assert(icache_req_queue_.size() <= icache_req_queue_size_ ,"ICache request queue overflows!");

        // Push new requests from back
        icache_req_queue_.emplace_back(inst_ptr);
        ILOG("Append ICache->L2Cache request queue!");
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

    // Append ICache resp queue
    void L2Cache::appendICacheRespQueue_(const olympia::InstPtr& inst_ptr) {
        sparta_assert(icache_resp_queue_.size() <= icache_resp_queue_size_ ,"ICache resp queue overflows!");

        // Push new resp to the icache_resp_queue_
        icache_resp_queue_.emplace_back(inst_ptr);
        ev_handle_l2cache_icache_resp_.schedule(sparta::Clock::Cycle(0));

        ILOG("Append L2Cache->ICache resp queue!");
    }

    // Append BIU req queue
    void L2Cache::appendBIUReqQueue_(const olympia::InstPtr& inst_ptr) {
        sparta_assert(biu_req_queue_.size() <= biu_req_queue_size_ ,"BIU req queue overflows!");

        // Push new request to the biu_req_queue_ if biu credits are available with the L2Cache
        biu_req_queue_.emplace_back(inst_ptr);
        ev_handle_l2cache_biu_req_.schedule(sparta::Clock::Cycle(0));

        ILOG("Append L2Cache->BIU req queue");
    }

    // Return the resp to the master units
    void L2Cache::sendOutResp_(const L2ArchUnit &unit, const olympia::InstPtr& instPtr) {
        // if (instPtr is originally from DCache)
        if (unit == L2ArchUnit::DCACHE) {
            appendDCacheRespQueue_(instPtr);
        }
        // if (instPtr is originally from ICache)
        else if (unit == L2ArchUnit::ICACHE) {
            appendICacheRespQueue_(instPtr);
        }
        else {
            sparta_assert(false, "Resp is being sent to a Unit that is not valid");
        }
    }

    // Send the request to the slave units
    void L2Cache::sendOutReq_(const L2ArchUnit &unit, const olympia::InstPtr& instPtr) {
        // if (instPtr is destined for BIU on L2Cache miss)
        if (unit == L2ArchUnit::BIU) {
            appendBIUReqQueue_(instPtr);
        }
        else {
            sparta_assert(false, "Request is being sent to a Unit that is not valid");
        }
    }

    // Select the Channel to pick the request from
    // Current options :
    //       BIU - P0
    //       DCache - P1 - RoundRobin Candidate
    //       DL1 - P1 - RoundRobin Candidate
    L2Cache::Channel L2Cache::arbitrateL2CacheAccessReqs_() {
        sparta_assert(icache_req_queue_.size() > 0 || dcache_req_queue_.size() > 0 || biu_resp_queue_.size() > 0,
                                "Arbitration failed: Reqest queues are empty!");

        Channel winner;

        if (pipeline_req_queue_.numFree() == 0) {

            // pipeline_req_queue_ is full, try again next cycle
            winner = Channel::NO_ACCESS;
        }
        // P0 priority to sevice the pending request in the buffer
        else if (!biu_resp_queue_.empty()) {

            winner = Channel::BIU;
            ILOG("Arbitration winner - BIU");
        }
        // RoundRobin for P1 Priority
        else if (channel_select_ == Channel::ICACHE) {
            // Set it up for the following arbitration request
            channel_select_ = Channel::DCACHE;
            winner = Channel::NO_ACCESS;

            if (!icache_req_queue_.empty()) {

                winner = Channel::ICACHE;
                ILOG("Arbitration winner - ICache");
            }
        }
        else if (channel_select_ == Channel::DCACHE) {

            // Set it up for the following arbitration request
            channel_select_ = Channel::ICACHE;
            winner = Channel::NO_ACCESS;

            if (!dcache_req_queue_.empty()) {

                winner = Channel::DCACHE;
                ILOG("Arbitration winner - DCache");
            }
        }
        else {
            sparta_assert(false, "Illegal else : Why is channel_select_ incorrectly set?");
        }

        return winner;
    }

    // Cache lookup for a HIT or MISS on a given request
    L2Cache::L2CacheState L2Cache::cacheLookup_(olympia::MemoryAccessInfoPtr mem_access_info_ptr) {
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
            ++l2_cache_hits_;
        }
        else if (cache_hit) {
            ILOG("Cache HIT: phyAddr=0x" << std::hex << phyAddr);
            ++l2_cache_hits_;
        }
        else {
            ILOG("Cache MISS: phyAddr=0x" << std::hex << phyAddr);
            ++l2_cache_misses_;
        }

        return (cache_hit ?
                      (L2CacheState::HIT)
                    : (L2CacheState::MISS));
    }

    // Allocating the cacheline in the L2 based on return from BIU/L3
    void L2Cache::reloadCache_(uint64_t phyAddr) {
        auto l2_cache_line = &l2_cache_->getLineForReplacementWithInvalidCheck(phyAddr);
        l2_cache_->allocateWithMRUUpdate(*l2_cache_line, phyAddr);
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
