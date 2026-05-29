// <L23.cpp> -*- C++ -*-

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/LogUtils.hpp"

#include "L23.hpp"


// initializing RauHashInstancePtr_ with NULL
olympia::RauHash* olympia::RauHash::RauHashInstancePtr_ = nullptr;

namespace olympia_mss
{
    sparta::SpartaSharedPointerAllocator<olympia::MemoryAccessInfo>  memory_access_allocator_(1000, 500);

    const char L23::name[] = "l23";

    ////////////////////////////////////////////////////////////////////////////////
    // Constructor
    ////////////////////////////////////////////////////////////////////////////////

    L23::L23(sparta::TreeNode *node, const L23ParameterSet *p) :
        sparta::Unit(node),

        // This allocator can be a member because L23InstInfo stays in the L23
        l23_inst_info_allocator{1000, 980},

        // Counters
        num_reqs_from_lsu_read_(&unit_stat_set_,
                     "num_reqs_from_lsu_read",
                     "The total number of instructions received by L23 from LSU Read",
                     sparta::Counter::COUNT_NORMAL),
        num_pf_reads_(&unit_stat_set_,
                     "num_pf_reads",
                     "The total number of PF reqs received from LSU",
                     sparta::Counter::COUNT_NORMAL),
        num_req_reads_(&unit_stat_set_,
                     "num_req_reads",
                     "The total number of demand read reqs received from LSU",
                     sparta::Counter::COUNT_NORMAL),
        num_reqs_from_lsu_write_(&unit_stat_set_,
                     "num_reqs_from_lsu_write",
                     "The total number of instructions received by L23 from LSU Write",
                     sparta::Counter::COUNT_NORMAL),
        num_reqs_from_icache_(&unit_stat_set_,
                     "num_reqs_from_icache",
                     "The total number of instructions received by L23 from ICache",
                     sparta::Counter::COUNT_NORMAL),
        num_reqs_from_rw_(&unit_stat_set_,
                     "num_reqs_from_rw",
                     "The total number of instructions received by L23 from RW Agent",
                     sparta::Counter::COUNT_NORMAL),
        num_reqs_to_biu_(&unit_stat_set_,
                     "num_reqs_to_biu",
                     "The total number of instructions forwarded from L23 to BIU",
                     sparta::Counter::COUNT_NORMAL),
        num_credits_from_biu_(&unit_stat_set_,
                     "num_credits_from_biu",
                     "The total number of credits received from BIU into L23",
                     sparta::Counter::COUNT_NORMAL),
        num_resps_from_biu_(&unit_stat_set_,
                     "num_resps_from_biu",
                     "The total number of instructions received from BIU into L23",
                     sparta::Counter::COUNT_NORMAL),
        num_resps_from_biu_pfack_(&unit_stat_set_,
                     "num_resps_from_biu_pfack",
                     "The total number of prefetch acks received from BIU into L23",
                     sparta::Counter::COUNT_NORMAL),
        num_resps_to_pfack_(&unit_stat_set_,
                     "num_resps_to_pfack",
                     "The total number of instructions forwarded from L23 to RW Agent",
                     sparta::Counter::COUNT_NORMAL),
        num_resps_to_rw_(&unit_stat_set_,
                     "num_resps_to_rw",
                     "The total number of instructions forwarded from L23 to RW Agent",
                     sparta::Counter::COUNT_NORMAL),
        num_resps_to_icache_(&unit_stat_set_,
                     "num_resps_to_icache",
                     "The total number of instructions forwarded from L23 to ICache",
                     sparta::Counter::COUNT_NORMAL),
        num_resps_to_lsu_read_(&unit_stat_set_,
                     "num_resps_to_lsu_read",
                     "The total number of instructions forwarded from L23 to LSU READ",
                     sparta::Counter::COUNT_NORMAL),
        cache_hits_(&unit_stat_set_,
                     "cache_hits",
                     "The total number L23 Cache Hits",
                     sparta::Counter::COUNT_NORMAL),
        cache_pf_hits_(&unit_stat_set_,
                     "cache_pf_hits",
                     "The total number L23 Cache Prefetch Hits",
                     sparta::Counter::COUNT_NORMAL),
        cache_misses_(&unit_stat_set_,
                     "cache_misses",
                     "The total number L23 Cache Misses",
                     sparta::Counter::COUNT_NORMAL),
        cache_reloads_(&unit_stat_set_,
                     "cache_reloads",
                     "The total number L23 Cache Reloads",
                     sparta::Counter::COUNT_NORMAL),
        dirty_evictions_(&unit_stat_set_,
                     "dirty_evictions",
                     "The total number L23 Dirty Evictions",
                     sparta::Counter::COUNT_NORMAL),
        clean_evictions_(&unit_stat_set_,
                     "clean_evictions",
                     "The total number L23 Clean Evictions",
                     sparta::Counter::COUNT_NORMAL),
        dropped_evictions_(&unit_stat_set_,
                     "dropped_evictions",
                     "The total number L23 Dropped Evictions",
                     sparta::Counter::COUNT_NORMAL),
        stat_pf_hits_(&unit_stat_set_,
                     "stat_pf_hits",
                     "The total number L23 Statistical Prefetcher Hits",
                     sparta::Counter::COUNT_NORMAL),
        stat_pf_misses_(&unit_stat_set_,
                     "stat_pf_misses",
                     "The total number L23 Statistical Prefetcher Misses",
                     sparta::Counter::COUNT_NORMAL),
        stat_pf_reloads_(&unit_stat_set_,
                     "stat_pf_reloads",
                     "The total number L23 Statistical Prefetcher Reloads",
                     sparta::Counter::COUNT_NORMAL),
        cache_level_(p->cache_level),

        lsu_read_req_queue_size_(p->lsu_read_req_queue_size),
        lsu_write_req_queue_size_(p->lsu_write_req_queue_size),
        icache_req_queue_size_(p->icache_req_queue_size),
        rw_req_queue_size_(p->rw_req_queue_size),
        
        lsu_read_req_queue_col_{node, "lsu_read_req_queue", lsu_read_req_queue_, static_cast<uint32_t>(lsu_read_req_queue_size_)},
        lsu_write_req_queue_col_{node, "lsu_write_req_queue", lsu_write_req_queue_, static_cast<uint32_t>(lsu_write_req_queue_size_)},
        icache_req_queue_col_{node, "icache_req_queue", icache_req_queue_, static_cast<uint32_t>(icache_req_queue_size_)},
        rw_req_queue_col_{node, "rw_req_queue", rw_req_queue_, static_cast<uint32_t>(rw_req_queue_size_)},

        biu_req_queue_size_(p->biu_req_queue_size),
        biu_req_queue_col_{node, "biu_req_queue", biu_req_queue_, static_cast<uint32_t>(biu_req_queue_size_)},

        biu_resp_queue_size_(p->biu_resp_queue_size),
        biu_resp_queue_col_{node, "biu_resp_queue", biu_resp_queue_, static_cast<uint32_t>(biu_resp_queue_size_)},

        lsu_read_resp_queue_size_(p->lsu_read_resp_queue_size),
        icache_resp_queue_size_(p->icache_resp_queue_size),
        rw_resp_queue_size_(p->rw_resp_queue_size),
        pfack_resp_queue_size_(p->pfack_resp_queue_size),

        lsu_read_resp_queue_col_{node, "lsu_read_resp_queue", lsu_read_resp_queue_, static_cast<uint32_t>(lsu_read_req_queue_size_)},
        icache_resp_queue_col_{node, "icache_resp_queue", icache_resp_queue_, static_cast<uint32_t>(icache_resp_queue_size_)},
        rw_resp_queue_col_{node, "rw_resp_queue", rw_resp_queue_, static_cast<uint32_t>(rw_resp_queue_size_)},
        pfack_resp_queue_col_{node, "pfack_resp_queue", pfack_resp_queue_, static_cast<uint32_t>(pfack_resp_queue_size_)},

	    max_bank_scheduling_delay_(p->max_bank_scheduling_delay),

	    l1_pipe_read_req_queue_size_(p->l1_pipe_read_req_queue_size),
        l1_pipe_write_req_queue_size_(p->l1_pipe_write_req_queue_size),
        biu_pipe_req_queue_size_(p->biu_pipe_req_queue_size),

        miss_pending_buffer_size_(p->miss_pending_buffer_size),

        num_banks_(p->num_banks),
        num_rows_per_bank_(p->num_rows_per_bank),

        pipe_latency_(p->pipe_latency),

        hash_algo_(set_HashAlgo_(p->hash_algo)),

        stat_prefetch_enable_(p->stat_prefetch_enable),
        gen_(p->stat_prefetch_seed),
        stat_prefetch_random_dist_(0.0, 1.0),
        stat_prefetch_rate_(p->stat_prefetch_rate),

        evict_clean_cl_(p->evict_clean_cl),

        is_rw_connected_(p->is_rw_connected),
        is_icache_connected_(p->is_icache_connected),
        is_lsu_read_connected_(p->is_lsu_read_connected),
        is_lsu_write_connected_(p->is_lsu_write_connected) {

    	// In Port Handler registration
        in_lsu_read_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L23, getReadReqFromLSU_, MemAccessPtr));

        in_lsu_write_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L23, getWriteReqFromLSU_, MemAccessPtr));

        in_icache_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L23, getReqFromICache_, MemAccessPtr));
        
        in_rw_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L23, getReqFromRW_, MemAccessPtr));

        in_biu_resp_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L23, getRespFromBIU_, MemAccessPtr));
        
        in_biu_pfack_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L23, getPFAckFromBIU_, MemAccessPtr));

        in_lsu_read_resp_credits_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L23, getCreditsFromLSU_, uint32_t));

        in_icache_resp_credits_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L23, getCreditsFromICache_, uint32_t));

        in_biu_credits_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(L23, getCreditsFromBIU_, uint32_t));

        // L23 cache config
        if (cache_level_ == "l2cache") {
            cache_ = getContainer()->getParent()->getChild("l2cache")->getResourceAs<olympia_mss::L23Cache>();
        }
        else if (cache_level_ == "l3cache") {
            cache_ = getContainer()->getParent()->getChild("l3cache")->getResourceAs<olympia_mss::L23Cache>();
        }
        else {
            sparta_assert(false, "Incorrect Cache Level")
        }

        line_size_ = cache_->getCacheLineSize();
        line_size_shift_ = log2(line_size_);
        sparta_assert(sparta::utils::is_power_of_2(line_size_), "Cache line size is not a power of 2");

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(L23, sendInitialCredits_));

        // Setup precedence for the entry in miss_pending_buffer_
        ev_create_req_.precedes(ev_handle_lsu_read_resp_);
        ev_create_req_.precedes(ev_handle_icache_resp_);
        ev_create_req_.precedes(ev_handle_rw_resp_);
        ev_create_req_.precedes(ev_handle_pfack_resp_);

        l23_bank_.reserve(num_banks_);
        for (uint32_t bank = 0; bank < num_banks_; ++bank) {
            l23_bank_.push_back(std::make_unique<L23Bank>(bank,
                                                        this,
                                                        node,
                                                        info_logger_,
                                                        debug_logger_));
        }

        ILOG("L23 construct: #" << node->getGroupIdx());
    }

    L23::~L23() {
        for (uint32_t bank_id = 0; bank_id < num_banks_; ++bank_id) {
            for (uint32_t row_id = 0; row_id < num_rows_per_bank_; ++row_id) {
                DLOG("l1_pipe_read_req_queue_" << " BANK ID: " << bank_id << " ROW ID: " << row_id << " Entries remaining: " << l23_bank_[bank_id]->l1_pipe_read_req_queue_[row_id]->size());
                auto& q0 = l23_bank_[bank_id]->l1_pipe_read_req_queue_[row_id];
                if (q0->size() > 0) {
                    for(auto iter = q0->begin(); iter != q0->end(); ++iter) {
                        DLOG(*iter);
                    }
                }
                
                DLOG("l1_pipe_write_req_queue_" << " BANK ID: " << bank_id << " ROW ID: " << row_id << " Entries remaining: " << l23_bank_[bank_id]->l1_pipe_write_req_queue_[row_id]->size());
                auto& q1 = l23_bank_[bank_id]->l1_pipe_write_req_queue_[row_id];
                if (q1->size() > 0) {
                    for(auto iter = q1->begin(); iter != q1->end(); ++iter) {
                        DLOG(*iter);
                    }
                }
                
                DLOG("biu_pipe_req_queue_" << " BANK ID: " << bank_id << " ROW ID: " << row_id << " Entries remaining: " << l23_bank_[bank_id]->biu_pipe_req_queue_[row_id]->size());
                auto& q2 = l23_bank_[bank_id]->biu_pipe_req_queue_[row_id];
                if (q2->size() > 0) {
                    for(auto iter = q2->begin(); iter != q2->end(); ++iter) {
                        DLOG(*iter);
                    }
                }
                
                DLOG("miss_pending_buffer_" << " BANK ID: " << bank_id << " ROW ID: " << row_id << " Entries remaining: " << l23_bank_[bank_id]->miss_pending_buffer_[row_id]->size());
                auto& q3 = l23_bank_[bank_id]->miss_pending_buffer_[row_id];
                if (q3->size() > 0) {
                    for(auto iter = q3->begin(); iter != q3->end(); ++iter) {
                        DLOG(*iter);
                    }
                }
                DLOG("\n");
            }
        }

        DLOG("icache_req_queue_" << " Entries remaining: " << icache_req_queue_.size());
        DLOG("lsu_read_req_queue_" << " Entries remaining: " << lsu_read_req_queue_.size());
        DLOG("lsu_write_req_queue_" << " Entries remaining: " << lsu_write_req_queue_.size());
        DLOG("rw_req_queue_" << " Entries remaining: " << rw_req_queue_.size());
        DLOG("biu_resp_queue_" << " Entries remaining: " << biu_resp_queue_.size());
    }

    L23Bank::L23Bank(const uint32_t& bank,
                         L23* l23,
                         sparta::TreeNode* node,
                         sparta::log::MessageSource& info_logger,
                         sparta::log::MessageSource& debug_logger) :
                                      bank_id_(bank),
                                      num_rows_(l23->num_rows_per_bank_),
                                      l23_(l23),
                                      info_logger_(info_logger),
                                      debug_logger_(debug_logger),
                                      unit_stat_set_(l23_->getStatisticSet()),
                                      unit_event_set_(l23_->getEventSet()),
                                      stages_(l23_->pipe_latency_),
                                      max_bank_scheduling_delay_(l23_->max_bank_scheduling_delay_),
                                      num_reqs_issued_(unit_stat_set_,
                                                       "bank" + std::to_string(bank_id_) + "_num_reqs_issued",
                                                       "Counter for total number of reqs issued for this bank",
                                                       sparta::Counter::COUNT_NORMAL),
                                      num_read_reqs_issued_(unit_stat_set_,
                                                       "bank" + std::to_string(bank_id_) + "_num_read_reqs_issued",
                                                       "Counter for total number of reqs issued for this bank",
                                                       sparta::Counter::COUNT_NORMAL),
                                      num_write_reqs_issued_(unit_stat_set_,
                                                       "bank" + std::to_string(bank_id_) + "_num_write_reqs_issued",
                                                       "Counter for total number of reqs issued for this bank",
                                                       sparta::Counter::COUNT_NORMAL),
                                      num_biu_reloads_issued_(unit_stat_set_,
                                                       "bank" + std::to_string(bank_id_) + "_num_biu_reloads_issued",
                                                       "Counter for total number of reqs issued for this bank",
                                                       sparta::Counter::COUNT_NORMAL),
                                      num_bank_stalls_(unit_stat_set_,
                                                       "bank" + std::to_string(bank_id_) + "_num_bank_stalls",
                                                       "Counter for total number stalls for this bank",
                                                       sparta::Counter::COUNT_NORMAL),
                                      num_miss_pending_buffer_hits_(unit_stat_set_,
                                                       "bank" + std::to_string(bank_id_) + "_num_miss_pending_buffer_hits",
                                                       "Counter for total number of hits in miss_pending_buffer_",
                                                       sparta::Counter::COUNT_NORMAL),
                                      num_read_mpb_hits_(unit_stat_set_,
                                                       "bank" + std::to_string(bank_id_) + "_num_read_mpb_hits",
                                                       "Counter for total number of read hits in miss_pending_buffer_",
                                                       sparta::Counter::COUNT_NORMAL),
                                      num_write_mpb_hits_(unit_stat_set_,
                                                       "bank" + std::to_string(bank_id_) + "_num_write_mpb_hits",
                                                       "Counter for total number of write hits in miss_pending_buffer_",
                                                       sparta::Counter::COUNT_NORMAL),
                                      num_pf_mpb_hits_(unit_stat_set_,
                                                       "bank" + std::to_string(bank_id_) + "_num_pf_mpb_hits",
                                                       "Counter for total number of prefetch hits in miss_pending_buffer_",
                                                       sparta::Counter::COUNT_NORMAL)
    {
        uev_issue_req_ = new sparta::UniqueEvent<>(unit_event_set_,
                                                  "bank" + std::to_string(bank_id_) + "_issue_req",
                                                  CREATE_SPARTA_HANDLER(L23Bank, issue_Req_));

        uev_update_bank_scheduling_delay_ = new sparta::UniqueEvent<>(unit_event_set_,
                                                  "bank" + std::to_string(bank_id_) + "_update_bank_scheduling_delay_",
                                                  CREATE_SPARTA_HANDLER(L23Bank, update_Bank_Scheduling_Delay_));

        uev_update_bank_scheduling_delay_->precedes(uev_issue_req_);

        for (uint32_t row_id = 0; row_id < num_rows_; ++row_id) {
            l23_pipeline_.push_back(std::unique_ptr<L23Pipeline> { new L23Pipeline("Bank" + std::to_string(bank_id_) + "_Row" + std::to_string(row_id) + "_L23_Pipeline",
                                                                 stages_.NUM_STAGES,
                                                                 l23_->getClock()) } );

            l1_pipe_read_req_queue_.push_back(std::unique_ptr<L23InstQueue> { new L23InstQueue("Bank" + std::to_string(bank_id_) + "_Row" + std::to_string(row_id) + "_L1_Pipeline_Read_Request_Queue",
                                                                            l23_->l1_pipe_read_req_queue_size_,
                                                                            l23_->getClock()) } );

            l1_pipe_write_req_queue_.push_back(std::unique_ptr<L23InstQueue> { new L23InstQueue("Bank" + std::to_string(bank_id_) + "_Row" + std::to_string(row_id) + "_L1_Pipeline_Write_Request_Queue",
                                                                              l23_->l1_pipe_write_req_queue_size_,
                                                                              l23_->getClock()) } );

            biu_pipe_req_queue_.push_back(std::unique_ptr<L23InstQueue> { new L23InstQueue("Bank" + std::to_string(bank_id_) + "_Row" + std::to_string(row_id) + "_BIU_Pipeline_Request_Queue",
                                                                         l23_->biu_pipe_req_queue_size_,
                                                                         l23_->getClock()) } );

            miss_pending_buffer_.push_back(std::unique_ptr<L23InstBuffer> { new L23InstBuffer("Bank" + std::to_string(bank_id_) + "_Row" + std::to_string(row_id) + "_Miss_Pending_Buffer",
                                                                           l23_->miss_pending_buffer_size_,
                                                                           l23_->getClock(),
                                                                           unit_stat_set_) } );

            // Enable collection
            l23_pipeline_[row_id]->enableCollection(node);

            l1_pipe_read_req_queue_[row_id]->enableCollection(node);
            l1_pipe_write_req_queue_[row_id]->enableCollection(node);
            biu_pipe_req_queue_[row_id]->enableCollection(node);

            miss_pending_buffer_[row_id]->enableCollection(node);

            // Allow the pipeline to create events and schedule work
            l23_pipeline_[row_id]->performOwnUpdates();

            // There can be situations where NOTHING is going on in the
            // simulator but forward progression of the pipeline elements.
            // In this case, the internal event for the pipeline will
            // be the only event keeping simulation alive.  Sparta
            // supports identifying non-essential events (by calling
            // setContinuing to false on any event).
            l23_pipeline_[row_id]->setContinuing(true);

            l23_pipeline_[row_id]->registerHandlerAtStage(stages_.CACHE_LOOKUP, CREATE_SPARTA_HANDLER(L23Bank, handleCacheLookup_));

            l23_pipeline_[row_id]->registerHandlerAtStage(stages_.MRQ_LOOKUP, CREATE_SPARTA_HANDLER(L23Bank, handleMRQLookup_));

            l23_pipeline_[row_id]->registerHandlerAtStage(stages_.CACHE_READ, CREATE_SPARTA_HANDLER(L23Bank, handleCacheRead_));

            uev_issue_req_->precedes(l23_->ev_create_req_);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Callbacks
    ////////////////////////////////////////////////////////////////////////////////
    // Sending Initial credits to ICache and LSU Read-Write
    void L23::sendInitialCredits_() {
        
        if (is_rw_connected_) {
            out_rw_req_credits_.send(rw_req_queue_size_);
            ILOG("Sending initial credits to RW Agent : " << rw_req_queue_size_);
        }

        if (is_icache_connected_) {
            out_icache_req_credits_.send(icache_req_queue_size_);
            ILOG("Sending initial credits to ICache : " << icache_req_queue_size_);
        }

        if (is_lsu_read_connected_) {
            out_lsu_read_credits_.send(lsu_read_req_queue_size_);
            ILOG("Sending initial credits to LSU_READ: " << lsu_read_req_queue_size_);
        }

        if (is_lsu_write_connected_) {
            out_lsu_write_credits_.send(lsu_write_req_queue_size_);
            ILOG("Sending initial credits to LSU_WRITE: " << lsu_write_req_queue_size_);
        }
    }

    // Receive new L23 read request from LSU
    void L23::getReadReqFromLSU_(const MemAccessPtr & mem_access_ptr) {

        ILOG("Request received from LSU on the read port MA : " << mem_access_ptr);

        appendLSUReadReqQueue_(mem_access_ptr);

        ev_handle_lsu_read_req_.schedule(sparta::Clock::Cycle(0));
        ++num_reqs_from_lsu_read_;

        if (mem_access_ptr->getReqType() == MemAccess::RequestType::PREFETCH) {
            ++num_pf_reads_;
        }
        else {
            ++num_req_reads_;
        }
    }

    // Receive new L23 write request from LSU
    void L23::getWriteReqFromLSU_(const MemAccessPtr & mem_access_ptr) {

        ILOG("Request received from LSU on the write port MA : " << mem_access_ptr);

        appendLSUWriteReqQueue_(mem_access_ptr);

        ev_handle_lsu_write_req_.schedule(sparta::Clock::Cycle(0));
        ++num_reqs_from_lsu_write_;
    }

    // Receive new L23 request from ICache
    void L23::getReqFromICache_(const MemAccessPtr & mem_access_ptr) {

        ILOG("Request received from ICache on the port MA : " << mem_access_ptr);

        appendICacheReqQueue_(mem_access_ptr);

        ev_handle_icache_req_.schedule(sparta::Clock::Cycle(0));
        ++num_reqs_from_icache_;
    }

    // Receive new L23 request from RW
    void L23::getReqFromRW_(const MemAccessPtr & mem_access_ptr) {

        ILOG("Request received from RW on the port MA : " << mem_access_ptr);

        appendRWReqQueue_(mem_access_ptr);

        ev_handle_rw_req_.schedule(sparta::Clock::Cycle(0));
        ++num_reqs_from_rw_;
    }

    // Handle BIU resp
    void L23::getRespFromBIU_(const MemAccessPtr & mem_access_ptr) {

        ILOG("Response received from BIU on the port MA : " << mem_access_ptr);

        appendBIURespQueue_(mem_access_ptr);

        // Schedule BIU resp handling event only when:
        // Request queue is not empty
        if (biu_resp_queue_.size() <= biu_resp_queue_size_) {
            ev_handle_biu_resp_.schedule(sparta::Clock::Cycle(1));
            ++num_resps_from_biu_;
        }
        else {
            sparta_assert(false, "This request cannot be serviced right now, L23 input buffer from BIU is already full!");
        }
    }

    // Handle BIU PF Ack
    void L23::getPFAckFromBIU_(const MemAccessPtr & mem_access_ptr) {

        ILOG("Response received from BIU on the PF Ack port MA : " << mem_access_ptr);

        appendPFAckQueue_(mem_access_ptr);
        appendBIURespQueue_(mem_access_ptr);

        // Schedule BIU resp handling event only when:
        // Request queue is not empty
        if (biu_resp_queue_.size() <= biu_resp_queue_size_) {
            ev_handle_biu_resp_.schedule(sparta::Clock::Cycle(1));
            ++num_resps_from_biu_;
            ++num_resps_from_biu_pfack_;
        }
        else {
            sparta_assert(false, "This request cannot be serviced right now, L23 input buffer from BIU is already full!");
        }
    }

    // Handle LSU credits
    void L23::getCreditsFromLSU_(const uint32_t & credits) {

        // Update the lsu credits
        lsu_read_resp_credits_ += credits;

        // Kickstart responding
        if (!lsu_read_resp_queue_.empty()) {
            ev_handle_lsu_read_resp_.schedule(sparta::Clock::Cycle(1));
        }

        ILOG("Credits received from LSU on the port : Current LSU credits available = " << lsu_read_resp_credits_);
    }

    // Handle ICache credits
    void L23::getCreditsFromICache_(const uint32_t & credits) {

        // Update the icache credits
        icache_resp_credits_ += credits;

        // Kickstart responding
        if (!icache_resp_queue_.empty()) {
            ev_handle_icache_resp_.schedule(sparta::Clock::Cycle(1));
        }

        ILOG("Credits received from ICache on the port : Current ICache credits available = " << icache_resp_credits_);
    }
    
    // Handle BIU credits
    void L23::getCreditsFromBIU_(const uint32_t & credits) {

        // Update the biu credits
        biu_credits_ = credits;

        // Kickstart the pipeline issueing
        for (uint32_t bank_id = 0; bank_id < num_banks_; ++bank_id) {
            for (uint32_t row_id = 0; row_id < num_rows_per_bank_; ++row_id) {
                l23_bank_[bank_id]->uev_issue_req_->schedule(sparta::Clock::Cycle(1));
            }
        }

        ILOG("Credits received from BIU on the port : Current BIU credits available = " << biu_credits_);
    }

    // Handle L23 read request from LSU
    void L23::handle_LSU_Read_Req_() {
        if (!lsu_read_req_queue_.empty()) {
            ev_create_req_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Handle L23 write request from LSU
    void L23::handle_LSU_Write_Req_() {
        if (!lsu_write_req_queue_.empty()) {
            ev_create_req_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Handle L23 request from ICache
    void L23::handle_ICache_Req_() {
        if (!icache_req_queue_.empty())  {
            ev_create_req_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Handle L23 request from RW
    void L23::handle_RW_Req_() {
        if (!rw_req_queue_.empty())  {
            ev_create_req_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Handle BIU->L23 response
    void L23::handle_BIU_Resp_() {
        
        ILOG("Trying to ready the miss_pending_buffer_ for biu resp : " << biu_resp_queue_.front());
        
        if (!biu_resp_queue_.empty())  {

            const MemAccessPtr &mem_access_ptr = biu_resp_queue_.front();

            auto is_cl_present = [mem_access_ptr] (auto reqPtr)
                        { return ((reqPtr->getMemAccessPtr()->getPAddr() == mem_access_ptr->getPAddr()) 
                                &&  (reqPtr->getL23MrqState() == MrqState::MISS || reqPtr->getL23MrqState() == MrqState::HIT)); };

            uint32_t bank_id = get_BankID_(mem_access_ptr);
            uint32_t row_id = get_RowID_(mem_access_ptr);
            auto req = std::find_if(l23_bank_[bank_id]->miss_pending_buffer_[row_id]->begin(),
                                    l23_bank_[bank_id]->miss_pending_buffer_[row_id]->end(),
                                    is_cl_present);

            if (req != l23_bank_[bank_id]->miss_pending_buffer_[row_id]->end()) {

                // Set the DataValid for the entries in the miss_pending_buffer for this BIU resp
                auto reqPtr = (*req);
                while(reqPtr != nullptr) {
                    ILOG("Request found in miss_pending_buffer_ of Bank ID : " << bank_id
                                                           << " Row ID : " << row_id
                                                         << " with REQ : " << reqPtr);
                    
                    // Only change the MRQ state when it isnt ready or issued
                    if (!(reqPtr->getL23MrqState() == MrqState::READY
                          || reqPtr->getL23MrqState() == MrqState::ISSUED)) {
                        
                        reqPtr->setL23MrqState(MrqState::READY);
                    }
                    
                    reqPtr->getMemAccessPtr()->setDataReady(true);
                    reqPtr = reqPtr->getNextMrqReq();
                }

                ILOG("Erasing biu_resp_queue_: " << biu_resp_queue_.front());
                biu_resp_queue_.erase(biu_resp_queue_.begin());

                // Reschedule the event to READY other entries in miss_pending_buffer_
                // based on the next biu_resp_queue_ packet
                if (!biu_resp_queue_.empty()) {
                    ev_handle_biu_resp_.schedule(sparta::Clock::Cycle(1));
                }
                ev_create_req_.schedule(sparta::Clock::Cycle(1));
            }
            else {
                sparta_assert(false, "Why did we not find a miss_pending_buffer_ entry that is a match for this BIU Resp! - " << biu_resp_queue_.front());
            }
        }
    }

    // Handle L23 request to BIU
    void L23::handle_BIU_Req_() {

        if (biu_credits_ > 0 && !biu_req_queue_.empty()) {

            const MemAccessPtr& ma_ptr = biu_req_queue_.front();

            out_biu_req_.send(ma_ptr);
            --biu_credits_;

            ++num_reqs_to_biu_;

            ILOG("Request sent to BIU : " << ma_ptr);
            ILOG("Current BIU credits available = " << biu_credits_);

            biu_req_queue_.erase(biu_req_queue_.begin());
        }

        if (!biu_req_queue_.empty()) {
            // Loop on biu_req_queue_ if the requests are present
            ev_handle_biu_req_.schedule(sparta::Clock::Cycle(1));
        }
    }

    // Returning resp to LSU
    void L23::handle_LSU_Read_Resp_() {
        // Early out if no credits. We need a kickstart when we receive a credit.
        if (lsu_read_resp_credits_ == 0) {
            return;
        }

        // Early out if empty
        if (lsu_read_resp_queue_.empty()) {
            return;
        }

        out_lsu_read_resp_.send(lsu_read_resp_queue_.front());
        --lsu_read_resp_credits_;
        ++num_resps_to_lsu_read_;

        ILOG("L23 Read Resp is sent to LSU : MA " << lsu_read_resp_queue_.front());
        lsu_read_resp_queue_.erase(lsu_read_resp_queue_.begin());

        if (!lsu_read_resp_queue_.empty()) {
            ev_handle_lsu_read_resp_.schedule(sparta::Clock::Cycle(1));
        }
    }

    // Returning resp to ICache
    void L23::handle_ICache_Resp_() {
        // Early out if no credits. We need a kickstart when we receive a credit.
        if (icache_resp_credits_ == 0) {
            return;
        }

        // Early out if empty
        if (icache_resp_queue_.empty()) {
            return;
        }

        out_icache_resp_.send(icache_resp_queue_.front());
        --icache_resp_credits_;
        ++num_resps_to_icache_;

        ILOG("L23 Resp is sent to ICache : MA " << icache_resp_queue_.front());
        icache_resp_queue_.erase(icache_resp_queue_.begin());

        if (!icache_resp_queue_.empty()) {
            ev_handle_icache_resp_.schedule(sparta::Clock::Cycle(1));
        }
    }

    // Returning resp to RW
    void L23::handle_RW_Resp_() {

        // Early out if empty
        if (rw_resp_queue_.empty()) {
            return;
        }

        out_rw_resp_.send(rw_resp_queue_.front());
        ++num_resps_to_rw_;

        ILOG("L23 Resp is sent to RW : MA " << rw_resp_queue_.front());
        rw_resp_queue_.erase(rw_resp_queue_.begin());

        if (!rw_resp_queue_.empty()) {
            ev_handle_rw_resp_.schedule(sparta::Clock::Cycle(1));
        }
    }

    // Returning resp to PF Ack
    void L23::handle_PFAck_Resp_() {

        // Early out if empty
        if (pfack_resp_queue_.empty()) {
            return;
        }

        out_pfack_resp_.send(pfack_resp_queue_.front());
        ++num_resps_to_pfack_;

        ILOG("L23 PF Ack Resp is sent: MA " << pfack_resp_queue_.front());
        pfack_resp_queue_.erase(pfack_resp_queue_.begin());

        if (!pfack_resp_queue_.empty()) {
            ev_handle_pfack_resp_.schedule(sparta::Clock::Cycle(1));
        }
    }

    // Handle arbitration and forward the req to pipeline_req_queue_
    void L23::create_Req_() {

        // [TODO] : Make sure the requests created are per slot.
        //          Also, Take into account the read and write restrictions, if any.
        uint32_t num_channels = static_cast<uint32_t>(Channel::NUM_CHANNELS);
        std::vector<bool> req_issued(num_channels, false);

        for (uint32_t bank_select = 0; bank_select < num_banks_; ++bank_select) {
            for (uint32_t row_select = 0; row_select < num_rows_per_bank_; ++row_select) {

                Channel arbitration_winner = arbitrateL23AccessReqs_(bank_select, row_select);

                if (arbitration_winner == Channel::NO_ACCESS) {
                    // Schedule a ev_create_req_ event again to see if the the new request
                    // from any of the requestors can be put into *_pipe_req_queue_
                    // ev_create_req_.schedule(sparta::Clock::Cycle(1));
                    DLOG("NO_ACCESS to banks, try again!");
                }
                else {
                    ILOG("Arbitration winner : " << arbitration_winner);

                    if (   arbitration_winner == Channel::BIU
                        && !req_issued[static_cast<uint32_t>(Channel::BIU)]) {

                        auto is_entry_ready = [] (auto reqPtr)
                                    { return (reqPtr->getL23MrqState() == MrqState::READY); };

                        auto req = std::find_if(l23_bank_[bank_select]->miss_pending_buffer_[row_select]->begin(),
                                                l23_bank_[bank_select]->miss_pending_buffer_[row_select]->end(),
                                                is_entry_ready);

                        if (req != l23_bank_[bank_select]->miss_pending_buffer_[row_select]->end()) {

                            if (l23_bank_[bank_select]->biu_pipe_req_queue_[row_select]->numFree() > 0) {

                                const L23InstInfoPtr& l23_info_ptr = (*req);
                                const MemAccessPtr& mem_access_ptr = l23_info_ptr->getMemAccessPtr();

                                ILOG("Request found in miss_pending_buffer_ Bank ID : " << bank_select
                                                                       <<  " Row ID : " << row_select
                                                                       << " with L23_Inst : " << l23_info_ptr);

                                // Send response out in parallel with the RELOAD request to pipeline
                                sendOutResp_(mem_access_ptr);

                                // Don't issue request for L3-PF requests to L2 Pipe
                                // Completion signal has been forwarded to LSU
                                bool l3pf_for_l2 = (mem_access_ptr->getReqType()  == MemAccess::RequestType::PREFETCH
                                                &&  mem_access_ptr->getSrcUnit()  == MemAccess::UnitName::LSU
                                                &&  mem_access_ptr->getDestUnit() == MemAccess::UnitName::L3CACHE);

                                // Don't issue request for LSU/L2 demands for L3 Pipe 
                                bool reload_for_l3 = (cache_level_ == "l3cache");

                                if (l3pf_for_l2 || reload_for_l3) {
                                    // Don't send this to RELOAD
                                    ILOG("Request is terminated!" << (l23_info_ptr));

                                    // Erase the entry from miss_pending_buffer_
                                    l23_info_ptr->setPrevMrqReq(nullptr);
                                    l23_info_ptr->setNextMrqReq(nullptr);
                                    l23_bank_[bank_select]->miss_pending_buffer_[row_select]->erase(req);
                                    ILOG("Erasing miss_pending_buffer_: " << l23_info_ptr);
                                }
                                else {
                                    // Send this to RELOAD
                                    l23_info_ptr->setL23MrqState(MrqState::ISSUED);
                                    l23_bank_[bank_select]->biu_pipe_req_queue_[row_select]->push(l23_info_ptr);
                                    ILOG("BIU request is sent to BIU_Pipe_Req_Q L23_Inst: " << (l23_info_ptr));
                                }

                                req_issued[static_cast<uint32_t>(Channel::BIU)] = true;
                            }
                        }
                    }
                    else if (arbitration_winner == Channel::ICACHE
                         && !req_issued[static_cast<uint32_t>(Channel::ICACHE)]) {

                        if (l23_bank_[bank_select]->l1_pipe_read_req_queue_[row_select]->numFree() > 0) {
                            const auto &reqPtr = sparta::allocate_sparta_shared_pointer<L23InstInfo>(l23_inst_info_allocator,
                                                                                                    icache_req_queue_.front(),
                                                                                                    bank_select,
                                                                                                    row_select);
                            MemAccessPtr mem_access_ptr = reqPtr->getMemAccessPtr();

                            l23_bank_[bank_select]->l1_pipe_read_req_queue_[row_select]->push(reqPtr);
                            ILOG("ICache request is sent to L1_Pipe_Read_Req_Q L23_Inst: " << reqPtr);

                            icache_req_queue_.erase(icache_req_queue_.begin());
                            req_issued[static_cast<uint32_t>(Channel::ICACHE)] = true;

                            // Send out the credits to ICache for credit management
                            out_icache_req_credits_.send(1);
                        }
                    }
                    else if (arbitration_winner == Channel::LSU_READ
                         && !req_issued[static_cast<uint32_t>(Channel::LSU_READ)]) {
                        
                        if (l23_bank_[bank_select]->l1_pipe_read_req_queue_[row_select]->numFree() > 0) {
                            
                            const auto &reqPtr = sparta::allocate_sparta_shared_pointer<L23InstInfo>(l23_inst_info_allocator,
                                                                                                    lsu_read_req_queue_.front(),
                                                                                                    bank_select,
                                                                                                    row_select);
                            MemAccessPtr mem_access_ptr = reqPtr->getMemAccessPtr();

                            l23_bank_[bank_select]->l1_pipe_read_req_queue_[row_select]->push(reqPtr);
                            ILOG("LSU READ request is sent to L1_Pipe_Read_Req_Q L23_Inst: " << reqPtr);

                            lsu_read_req_queue_.erase(lsu_read_req_queue_.begin());
                            req_issued[static_cast<uint32_t>(Channel::LSU_READ)] = true;

                            // Send out the credits to LSU read for credit management
                            out_lsu_read_credits_.send(1);
                        }

                            const auto &reqPtr = sparta::allocate_sparta_shared_pointer<L23InstInfo>(l23_inst_info_allocator,
                                                                                                    lsu_read_req_queue_.front(),
                                                                                                    bank_select,
                                                                                                    row_select);
                            MemAccessPtr mem_access_ptr = reqPtr->getMemAccessPtr();

                            l23_bank_[bank_select]->l1_pipe_read_req_queue_[row_select]->push(reqPtr);
                            ILOG("LSU READ request is sent to L1_Pipe_Read_Req_Q L23_Inst: " << reqPtr);

                            lsu_read_req_queue_.erase(lsu_read_req_queue_.begin());
                            req_issued[static_cast<uint32_t>(Channel::LSU_READ)] = true;

                            // Send out the credits to LSU read for credit management
                            out_lsu_read_credits_.send(1);
                        }
                    }
                    else if (arbitration_winner == Channel::LSU_WRITE
                         && !req_issued[static_cast<uint32_t>(Channel::LSU_WRITE)]) {

                        if (l23_bank_[bank_select]->l1_pipe_write_req_queue_[row_select]->numFree() > 0) {
                            const auto &reqPtr = sparta::allocate_sparta_shared_pointer<L23InstInfo>(l23_inst_info_allocator,
                                                                                                    lsu_write_req_queue_.front(),
                                                                                                    bank_select,
                                                                                                    row_select);
                            MemAccessPtr mem_access_ptr = reqPtr->getMemAccessPtr();

                            l23_bank_[bank_select]->l1_pipe_write_req_queue_[row_select]->push(reqPtr);
                            ILOG("LSU Write request is sent to L1_Pipe_Write_Req_Q L23_Inst: " << reqPtr);

                            lsu_write_req_queue_.erase(lsu_write_req_queue_.begin());
                            req_issued[static_cast<uint32_t>(Channel::LSU_WRITE)] = true;

                            // Send out the credits to LSU write for credit management
                            out_lsu_write_credits_.send(1);
                        }
                    }
                    else if (arbitration_winner == Channel::READWRITE
                         && !req_issued[static_cast<uint32_t>(Channel::READWRITE)]) {
                        
                        if (rw_req_queue_.front()->getReqType() == MemAccess::RequestType::READ) {

                            if (l23_bank_[bank_select]->l1_pipe_read_req_queue_[row_select]->numFree() > 0) {
                                const auto &reqPtr = sparta::allocate_sparta_shared_pointer<L23InstInfo>(l23_inst_info_allocator,
                                                                                                        rw_req_queue_.front(),
                                                                                                        bank_select,
                                                                                                        row_select);
                                MemAccessPtr mem_access_ptr = reqPtr->getMemAccessPtr();

                                l23_bank_[bank_select]->l1_pipe_read_req_queue_[row_select]->push(reqPtr);
                                ILOG("RW READ request is sent to L1_Pipe_Read_Req_Q L23_Inst: " << reqPtr);

                                rw_req_queue_.erase(rw_req_queue_.begin());
                                req_issued[static_cast<uint32_t>(Channel::READWRITE)] = true;

                                // Send out the credits to RW read for credit management
                                out_rw_req_credits_.send(1);
                            }
                        }
                        else if (rw_req_queue_.front()->getReqType() == MemAccess::RequestType::WRITE) {
                            
                            if (l23_bank_[bank_select]->l1_pipe_write_req_queue_[row_select]->numFree() > 0) {
                                const auto &reqPtr = sparta::allocate_sparta_shared_pointer<L23InstInfo>(l23_inst_info_allocator,
                                                                                                        rw_req_queue_.front(),
                                                                                                        bank_select,
                                                                                                        row_select);
                                MemAccessPtr mem_access_ptr = reqPtr->getMemAccessPtr();
                                mem_access_ptr->setDestUnit(L23UnitName::L3CACHE);

                                l23_bank_[bank_select]->l1_pipe_write_req_queue_[row_select]->push(reqPtr);
                                ILOG("RW Write request is sent to L1_Pipe_Write_Req_Q L2_Inst: " << reqPtr);

                                rw_req_queue_.erase(rw_req_queue_.begin());
                                req_issued[static_cast<uint32_t>(Channel::READWRITE)] = true;

                                // Send out the credits to RW write for credit management
                                out_rw_req_credits_.send(1);
                            }
                        }
                        else if (rw_req_queue_.front()->getReqType() == MemAccess::RequestType::PREFETCH) {

                            if (l23_bank_[bank_select]->l1_pipe_read_req_queue_[row_select]->numFree() > 0) {
                                const auto &reqPtr = sparta::allocate_sparta_shared_pointer<L23InstInfo>(l23_inst_info_allocator,
                                                                                                        rw_req_queue_.front(),
                                                                                                        bank_select,
                                                                                                        row_select);
                                MemAccessPtr mem_access_ptr = reqPtr->getMemAccessPtr();

                                l23_bank_[bank_select]->l1_pipe_read_req_queue_[row_select]->push(reqPtr);
                                ILOG("RW PREFETCH request is sent to L1_Pipe_Read_Req_Q L23_Inst: " << reqPtr);

                                rw_req_queue_.erase(rw_req_queue_.begin());
                                req_issued[static_cast<uint32_t>(Channel::READWRITE)] = true;

                                // Send out the credits to RW read for credit management
                                out_rw_req_credits_.send(1);
                            }
                        }
                        else {
                            sparta_assert(false, "Incorrect Request Type on RW port");
                        }
                    }

                    l23_bank_[bank_select]->uev_issue_req_->schedule(sparta::Clock::Cycle(1));
                }
            }
        }

        // Schedule a ev_create_req_ event again to see if the the new request
        // from any of the requestors can be put into *_pipe_req_queue_
        if (   is_MRQ_Ready_()
            || !icache_req_queue_.empty()
            || !lsu_read_req_queue_.empty()
            || !lsu_write_req_queue_.empty()
            || !rw_req_queue_.empty() ) {
            ILOG("More requests to be created next cycle");
            ev_create_req_.schedule(sparta::Clock::Cycle(1));
        }
    }

    void L23Bank::issue_Req_() {

        // Append the request to a pipeline if the *_pipe_req_queue_ is not empty
        // and l23_pipeline_ has credits available
        for (uint32_t row_id = 0; row_id < num_rows_; ++row_id) {

            // BIU gets priority when you have requests from L1 and BIU both present
            // 
            // NOTE: This category does not require backend resources and hence,
            // we will not increment inflight_reqs
            if (!biu_pipe_req_queue_[row_id]->empty()) {

                if (bank_scheduling_delay_counter_ == 0) {

                    const L23InstInfoPtr& l23_inst_ptr = biu_pipe_req_queue_[row_id]->front();
                    sparta_assert(l23_inst_ptr->getL23InstBankID() == bank_id_ && l23_inst_ptr->getL23InstRowID() == row_id,
                                    "How this instruction end up over here : " << l23_inst_ptr);
                    l23_inst_ptr->setL23PipeState(PipeState::RELOAD_ISSUED);

                    l23_pipeline_[row_id]->append(l23_inst_ptr);
                    ILOG("BIU Resp is sent to Pipeline of Bank ID : " << bank_id_
                                                    <<  " Row_ID : " << row_id
                                                    << " L23_Inst : " << l23_inst_ptr);

                    biu_pipe_req_queue_[row_id]->pop();
                    ++num_biu_reloads_issued_;
                    bank_scheduling_delay_counter_ = max_bank_scheduling_delay_;
                }

                // Wait scheduling delay
                uev_update_bank_scheduling_delay_->schedule(sparta::Clock::Cycle(1));
            }
            else if (l23_->cache_level_ == "l3cache" 
                && !l1_pipe_write_req_queue_[row_id]->empty()) {

                if (bank_scheduling_delay_counter_ == 0) {
                    const L23InstInfoPtr& l23_inst_ptr = l1_pipe_write_req_queue_[row_id]->front();
                    sparta_assert(l23_inst_ptr->getL23InstBankID() == bank_id_ && l23_inst_ptr->getL23InstRowID() == row_id,
                                    "How this instruction end up over here : " << l23_inst_ptr);
                    l23_inst_ptr->setL23PipeState(PipeState::RELOAD_ISSUED);

                    l23_pipeline_[row_id]->append(l23_inst_ptr);
                    ILOG("Write Request is sent to Pipeline Bank ID : " << bank_id_
                                                        <<  " Row_ID : " << row_id
                                                        << " L23_Inst : " << l23_inst_ptr);

                    l1_pipe_write_req_queue_[row_id]->pop();
                    ++num_write_reqs_issued_;
                    bank_scheduling_delay_counter_ = max_bank_scheduling_delay_;
                }

                // Wait scheduling delay
                uev_update_bank_scheduling_delay_->schedule(sparta::Clock::Cycle(1));
            }
            else {
                if (hasCreditsForPipelineIssue_(row_id)
                && (!l1_pipe_read_req_queue_[row_id]->empty()
                 || !l1_pipe_write_req_queue_[row_id]->empty()) ) {

                    if (!l1_pipe_read_req_queue_[row_id]->empty()) {

                        if (bank_scheduling_delay_counter_ == 0) {

                            const L23InstInfoPtr& l23_inst_ptr = l1_pipe_read_req_queue_[row_id]->front();
                            sparta_assert(l23_inst_ptr->getL23InstBankID() == bank_id_ && l23_inst_ptr->getL23InstRowID() == row_id,
                                            "How this instruction end up over here : " << l23_inst_ptr);
                            l23_inst_ptr->setL23PipeState(PipeState::REQ_ISSUED);

                            l23_pipeline_[row_id]->append(l23_inst_ptr);
                            ++l23_->inFlight_reqs_;

                            ILOG("Read Request is sent to Pipeline of Bank ID : " << bank_id_
                                                                    <<  " Row_ID : " << row_id
                                                                    << " L23_Inst : " << l23_inst_ptr);

                            l1_pipe_read_req_queue_[row_id]->pop();
                            ++num_read_reqs_issued_;
                            bank_scheduling_delay_counter_ = max_bank_scheduling_delay_;
                        }

                        // Wait scheduling delay
                        uev_update_bank_scheduling_delay_->schedule(sparta::Clock::Cycle(1));
                    }
                    else if (!l1_pipe_write_req_queue_[row_id]->empty()) {

                        if (bank_scheduling_delay_counter_ == 0) {
                            const L23InstInfoPtr& l23_inst_ptr = l1_pipe_write_req_queue_[row_id]->front();
                            sparta_assert(l23_inst_ptr->getL23InstBankID() == bank_id_ && l23_inst_ptr->getL23InstRowID() == row_id,
                                            "How this instruction end up over here : " << l23_inst_ptr);
                            if (l23_->cache_level_ == "l2cache") {
                                l23_inst_ptr->setL23PipeState(PipeState::REQ_ISSUED);
                            }
                            else {
                                sparta_assert(false, "Incorrect Cache Level")
                            }

                            l23_pipeline_[row_id]->append(l23_inst_ptr);
                            ++l23_->inFlight_reqs_;

                            ILOG("Write Request is sent to Pipeline Bank ID : " << bank_id_
                                                                <<  " Row_ID : " << row_id
                                                                << " L23_Inst : " << l23_inst_ptr);

                            l1_pipe_write_req_queue_[row_id]->pop();
                            ++num_write_reqs_issued_;
                            bank_scheduling_delay_counter_ = max_bank_scheduling_delay_;
                        }

                        // Wait scheduling delay
                        uev_update_bank_scheduling_delay_->schedule(sparta::Clock::Cycle(1));
                    }
                    else {
                        sparta_assert(false, "No request to issue to pipeline?");
                    }
                    ++num_reqs_issued_;
                }
            }

            if (!hasCreditsForPipelineIssue_(row_id)) {
                ++num_bank_stalls_;
            }
        }

        // Checking for the queue empty again before scheduling the event for the next clock cycle
        for (uint32_t row_id = 0; row_id < num_rows_; ++row_id) {
            if (!l1_pipe_read_req_queue_[row_id]->empty()
             || !l1_pipe_write_req_queue_[row_id]->empty()
             || !biu_pipe_req_queue_[row_id]->empty()) {

                uev_issue_req_->schedule(sparta::Clock::Cycle(1));
            }
        }
    }

    // Pipeline Stage CACHE_LOOKUP
    void L23Bank::handleCacheLookup_() {

        for (uint32_t row_id = 0; row_id < num_rows_; ++row_id) {

            // Check if the pipeline stage for this row is Valid
            if (l23_pipeline_[row_id]->isValid(stages_.CACHE_LOOKUP)) {

                auto req = l23_pipeline_[row_id]->at(stages_.CACHE_LOOKUP);

                ILOG("CACHE_LOOKUP Start : Bank ID : " << bank_id_
                                     << " : Row ID : " << row_id
                                     <<    " - Req : " << req);

                if (req->getL23PipeState() == PipeState::REQ_ISSUED
                 || req->getL23PipeState() == PipeState::RELOAD_ISSUED) {

                    PipeState cacheLookUpResult = l23_->cache_->lookup_mem_acess(req->getMemAccessPtr())
                                                            ? PipeState::HIT
                                                            : PipeState::MISS;

                    // Access cache, and check cache hit or miss
                    if (req->getL23PipeState() == PipeState::RELOAD_ISSUED) {

                        ++l23_->cache_reloads_;

                        // First reload to the line will miss, subsequent should be hits
                        if (cacheLookUpResult == PipeState::MISS) {
                            // Reload cache line
                            l23_->reloadL23Cache_(req->getMemAccessPtr());
                            req->setL23PipeState(PipeState::RELOAD_STARTED);

                            ILOG("Reload started - L23_Inst : " << req);
                        }
                        else {
                            // CCC-790 + CAP-247
                            // sparta_assert(false, "Why was this RELOAD issued if it is a HIT in CACHE");

                            // Mark the instruction complete as the reload has been returned to the requestor
                            // and also written to the Cache
                            req->setL23PipeState(PipeState::COMPLETED);
                        }
                    }
                    else { //req->getL23PipeState() == PipeState::REQ_ISSUED
                        auto src_unit = req->getMemAccessPtr()->getSrcUnit();

                        if ((src_unit == MemAccess::UnitName::LSU || src_unit == MemAccess::UnitName::L2CACHE)
                         && cacheLookUpResult == PipeState::MISS
                         && l23_->stat_prefetch_enable_) {

                            // Consult the Statistical Prefetcher
                            bool is_hit = l23_->stat_prefetch_random_dist_(l23_->gen_) <= l23_->stat_prefetch_rate_;

                            if (is_hit) {
                                cacheLookUpResult = PipeState::STAT_PF_HIT;
                                l23_->reloadL23Cache_(req->getMemAccessPtr());
                                ++l23_->stat_pf_hits_;
                                ++l23_->stat_pf_reloads_;
                            }
                            else {
                                cacheLookUpResult = PipeState::STAT_PF_MISS;
                                ++l23_->stat_pf_misses_;
                            }
                        }

                        // Record a hit miss state here, it may change below this point
                        if (cacheLookUpResult == PipeState::MISS) {
                            ++l23_->cache_misses_;
                        }
                        else if (cacheLookUpResult == PipeState::HIT) {
                            ++l23_->cache_hits_;
                        }

                        // Update L23 Pipe State
                        req->setL23PipeState(cacheLookUpResult);
                    }
                }
                else {
                    sparta_assert(false, "illegal getL23PipeState() : " << req->getL23PipeState());
                }

                ILOG("CACHE_LOOKUP Result : Bank ID : " << bank_id_
                                      << " : Row ID : " << row_id
                                      <<    " - Req : " << req);
            }
        }
    }

    // Pipeline Stage MRQ_LOOKUP
    void L23Bank::handleMRQLookup_() {

        for (uint32_t row_id = 0; row_id < num_rows_; ++row_id) {

            // Check if the pipeline stage for this row is Valid
            if (l23_pipeline_[row_id]->isValid(stages_.MRQ_LOOKUP)) {

                auto req = l23_pipeline_[row_id]->at(stages_.MRQ_LOOKUP);
                ILOG("MRQ_LOOKUP Start : Bank ID : " << bank_id_
                                     << " Row ID : " << row_id
                                     <<  " - Req : " << req);

                if (((req->getL23PipeState() == PipeState::MISS) || (req->getL23PipeState() == PipeState::STAT_PF_MISS))
                  && (req->getL23MrqState() == MrqState::NA)) {

                    const MemAccessPtr& mem_access_ptr = req->getMemAccessPtr();
                    const MemAccessPtr& req_ma_ptr = sparta::allocate_sparta_shared_pointer<MemAccess> (memory_access_allocator_, mem_access_ptr->getPAddr());

                    // Set the source unit to final destunit determined earlier
                    if (mem_access_ptr->getSrcUnit() == L23UnitName::LSU ||
                        mem_access_ptr->getSrcUnit() == L23UnitName::ICACHE) {
                        req_ma_ptr->setSrcUnit(L23UnitName::L2CACHE);
                    }
                    else if (mem_access_ptr->getSrcUnit() == L23UnitName::L2CACHE) {
                        req_ma_ptr->setSrcUnit(L23UnitName::L3CACHE);
                    }
                    else {
                        sparta_assert(false, "Incorrect source unit");
                    }

                    // Set Destination for this request to BIU
                    req_ma_ptr->setDestUnit(mem_access_ptr->getDestUnit());

                    // Set Request Type
                    if (mem_access_ptr->getReqType() == MemAccess::RequestType::PREFETCH) {
                        req_ma_ptr->setReqType(MemAccess::RequestType::PREFETCH);
                    }
                    else {
                        req_ma_ptr->setReqType(MemAccess::RequestType::READ);
                    }

                    // Handle the miss instruction by storing it aside while waiting
                    // for lower level memory to return
                    if (miss_pending_buffer_[row_id]->size() < l23_->miss_pending_buffer_size_) {

                        ILOG("Storing the CACHE MISS in miss_pending_buffer_ : Bank ID : " << bank_id_
                                                                           << " Row ID : " << row_id
                                                                           <<  " - Req : " << req);
                        miss_pending_buffer_[row_id]->push_back(req);
                    }
                    else {
                        sparta_assert(false, "No space in miss_pending_buffer_! Why did the frontend issue push the request onto l23_pipeline_?");
                    }

                    // Function to check if the request to the given cacheline is present in the miss_pending_buffer_
                    auto is_cl_present = [&req] (auto reqPtr)
                                    { return (req != reqPtr && reqPtr->getPAddr() == req->getPAddr()); };

                    // Send out the request to BIU for a cache MISS if it is not sent out already
                    auto reqIter = std::find_if(miss_pending_buffer_[row_id]->rbegin(),
                                                miss_pending_buffer_[row_id]->rend(),
                                                is_cl_present);

                    if (reqIter == miss_pending_buffer_[row_id]->rend()) {
                        req->setL23MrqState(MrqState::MISS);
                        l23_->sendOutReq_(req_ma_ptr);
                    }
                    else {
                        req->setL23MrqState(MrqState::HIT);
                        
                        // Collect stats for upgrades
                        ++num_miss_pending_buffer_hits_;

                        switch(mem_access_ptr->getReqType()) {
                            case MemAccess::RequestType::READ: [[likely]]
                                ++num_read_mpb_hits_;
                                break;
                            case MemAccess::RequestType::WRITE:
                                ++num_write_mpb_hits_;
                                break;
                            default:
                                ++num_pf_mpb_hits_;
                                break;
                        }

                        // If the request found in the miss_pending_buffer has the data ready,
                        // then set the data_ready for this miss in the cache.
                        //
                        // This indicates that data has been received by the CACHE from BIU,
                        // but has not been "RELOADED" into the cache yet.
                        if ((*reqIter)->getMemAccessPtr()->isDataReady() || 
                            (*reqIter)->getL23MrqState() == MrqState::READY) {
                            
                            req->setL23MrqState(MrqState::READY);
                            mem_access_ptr->setDataReady(true);

                            // Handle this case just like a biu response has arrived
                            // for the given cacheline and schedule a create_req event next cycle
                            l23_->ev_create_req_.schedule(sparta::Clock::Cycle(1));
                        }

                        // Found a request to same cacheLine.
                        // Link the current request to the last pending request
                        (*reqIter)->setNextMrqReq(req);
                        req->setPrevMrqReq(*reqIter);
                    }

                    l23_pipeline_[row_id]->invalidateStage(stages_.MRQ_LOOKUP);
                    --l23_->inFlight_reqs_;
                }
                else if (req->getL23PipeState() == PipeState::HIT
                      || req->getL23PipeState() == PipeState::STAT_PF_HIT) { // CacheLookup is HIT

                    // Decrementing the inflight reqs here.
                    // Essentially saying that this request is not going to use
                    // any backend resource in L23
                    --l23_->inFlight_reqs_;
                }
                else if (req->getL23PipeState() == PipeState::RELOAD_STARTED
                      || req->getL23PipeState() == PipeState::COMPLETED) {
                    
                    // RELOAD for L3 is just a reload, and not a miss
                    if (l23_->cache_level_ != "l3cache") {
                        
                        // Function to check if the request to the given req is present in the miss_pending_buffer_
                        auto is_req_present = [&req] (auto reqPtr) { return (req == reqPtr); };
                        auto reqIter = std::find_if(miss_pending_buffer_[row_id]->begin(),
                                                    miss_pending_buffer_[row_id]->end(),
                                                    is_req_present);
                        
                        // Erase the entry from miss_pending_buffer_
                        if (reqIter != miss_pending_buffer_[row_id]->end()) {
                            req->setPrevMrqReq(nullptr);
                            req->setNextMrqReq(nullptr);
                            DLOG("Matched Entry: " << *reqIter);
                            ILOG("Erasing miss_pending_buffer_: " << req);
                            miss_pending_buffer_[row_id]->erase(reqIter);
                        }
                        else {
                            sparta_assert(false, "Why did we not find this entry?");
                        }  
                    }
                }

                ILOG("MRQ_LOOKUP Result: Bank ID : " << bank_id_
                                     << " Row ID : " << row_id
                                     <<  " - Req : " << req);
            }
        }
    }

    // Pipeline Stage CACHE_READ
    void L23Bank::handleCacheRead_() {

        for (uint32_t row_id = 0; row_id < num_rows_; ++row_id) {

            // Check if the pipeline stage for this row is Valid
            if (l23_pipeline_[row_id]->isValid(stages_.CACHE_READ)) {

                auto req = l23_pipeline_[row_id]->at(stages_.CACHE_READ);
                ILOG("Pipeline stage CACHE_READ : Bank ID : " << bank_id_
                                              << " Row ID : " << row_id
                                               << " - Req : " << req);

                if (req->getL23PipeState() != PipeState::COMPLETED) {

                    // This request to access cache came from LSU or ICache to do a cache lookup.
                    // It was either a miss or hit based on cacheLookup_() in the previous stage of the pipeline
                    if (req->getL23PipeState() == PipeState::HIT
                     || req->getL23PipeState() == PipeState::STAT_PF_HIT) {
                        // If it was originally a miss in L23, on return from BIU, it's SrcUnit is set to BIU
                        // and DestUnit to whatever the original SrcUnit was.
                        //
                        // If it was a hit in L23, return the request back to where it originally came from.
                        //
                        // Send out the resp to the original SrcUnit -- which is now the DestUnit.
                        l23_->sendOutResp_(req->getMemAccessPtr());
                    }
                    else if (req->getL23PipeState() == PipeState::RELOAD_STARTED) {
                        // Reload is complete here
                        ILOG("Reload completed - L23_Inst : Bank ID : " << bank_id_
                                                      << " Row ID : " << row_id
                                                       << " - Req : " << req);
                    }
                    else {
                        // This is an illegal case
                        sparta_assert(false, "Why has this req reached Cache Read Stage? \n" << req);
                    }

                    req->setL23PipeState(PipeState::COMPLETED);
                }
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Regular Function/Subroutine Call
    ////////////////////////////////////////////////////////////////////////////////

    // Append L23 request queue for read reqs from LSU
    void L23::appendLSUReadReqQueue_(const MemAccessPtr& mem_access_ptr) {
        sparta_assert(lsu_read_req_queue_.size() < lsu_read_req_queue_size_ ,"LSU Read request queue overflows!");

        // Push new requests from back
        lsu_read_req_queue_.emplace_back(mem_access_ptr);
        ILOG("Append LSU->L23 read request queue!");
    }

    // Append L23 request queue for write reqs from LSU
    void L23::appendLSUWriteReqQueue_(const MemAccessPtr& mem_access_ptr) {
        sparta_assert(lsu_write_req_queue_.size() < lsu_write_req_queue_size_ ,"LSU Write request queue overflows!");

        // Push new requests from back
        lsu_write_req_queue_.emplace_back(mem_access_ptr);
        ILOG("Append LSU->L23 write request queue!");
    }

    // Append L23 request queue for reqs from RW Agent
    void L23::appendRWReqQueue_(const MemAccessPtr& mem_access_ptr) {
        sparta_assert(icache_req_queue_.size() < rw_req_queue_size_ ,"RW request queue overflows!");

        // Push new requests from back
        rw_req_queue_.emplace_back(mem_access_ptr);
        ILOG("Append RW->L23 request queue!");
    }

    // Append L23 request queue for reqs from ICache
    void L23::appendICacheReqQueue_(const MemAccessPtr& mem_access_ptr) {
        sparta_assert(icache_req_queue_.size() < icache_req_queue_size_ ,"ICache request queue overflows!");

        // Push new requests from back
        icache_req_queue_.emplace_back(mem_access_ptr);
        ILOG("Append ICache->L23 request queue!");
    }

    // Append BIU resp queue
    void L23::appendBIURespQueue_(const MemAccessPtr& mem_access_ptr) {
        sparta_assert(biu_resp_queue_.size() < biu_resp_queue_size_ ,"BIU resp queue overflows!");

        // Push new requests from back
        biu_resp_queue_.emplace_back(mem_access_ptr);

        ILOG("Append BIU->L23 resp queue!" << mem_access_ptr);
        ILOG("biu_resp_queue_ size = " << biu_resp_queue_.size());
    }

    // Append LSU read resp queue
    void L23::appendLSUReadRespQueue_(const MemAccessPtr& mem_access_ptr) {
        sparta_assert(lsu_read_resp_queue_.size() < lsu_read_resp_queue_size_ ,"LSU read resp queue overflows!");

        // Push new resp to the lsu_read_resp_queue_
        lsu_read_resp_queue_.emplace_back(mem_access_ptr);
        ev_handle_lsu_read_resp_.schedule(sparta::Clock::Cycle(0));

        ILOG("Append L23->LSU read resp queue!");
    }

    // Append ICache resp queue
    void L23::appendICacheRespQueue_(const MemAccessPtr& mem_access_ptr) {
        sparta_assert(icache_resp_queue_.size() < icache_resp_queue_size_ ,"ICache resp queue overflows!");

        // Push new resp to the icache_resp_queue_
        icache_resp_queue_.emplace_back(mem_access_ptr);
        ev_handle_icache_resp_.schedule(sparta::Clock::Cycle(0));

        ILOG("Append L23->ICache resp queue!");
    }

    // Append RW resp queue
    void L23::appendRWRespQueue_(const MemAccessPtr& mem_access_ptr) {
        sparta_assert(rw_resp_queue_.size() < rw_resp_queue_size_ ,"RW resp queue overflows!");

        // Push new resp to the rw_resp_queue_
        rw_resp_queue_.emplace_back(mem_access_ptr);
        ev_handle_rw_resp_.schedule(sparta::Clock::Cycle(0));

        ILOG("Append L23->RW resp queue!");
    }

    // Append RW resp queue
    void L23::appendPFAckQueue_(const MemAccessPtr& mem_access_ptr) {
        sparta_assert(pfack_resp_queue_.size() < pfack_resp_queue_size_ ,"PF Ack resp queue overflows!");

        // Push new resp to the rw_resp_queue_
        pfack_resp_queue_.emplace_back(mem_access_ptr);
        ev_handle_pfack_resp_.schedule(sparta::Clock::Cycle(0));

        ILOG("Append L23->PFACK resp queue!");
    }

    // Append BIU req queue
    void L23::appendBIUReqQueue_(const MemAccessPtr& mem_access_ptr) {
        if (biu_req_queue_.size() == biu_req_queue_size_) {
            sparta_assert(biu_req_queue_.size() < biu_req_queue_size_ ,"BIU req queue overflows!");
        }
        // Push new request to the biu_req_queue_ if biu credits are available with the L23
        biu_req_queue_.emplace_back(mem_access_ptr);
        ev_handle_biu_req_.schedule(sparta::Clock::Cycle(1));

        ILOG("Append L23->BIU req queue");
    }

    // Return the resp to the master units
    void L23::sendOutResp_(const MemAccessPtr& mem_access_ptr) {

        const L23UnitName &dest_unit = mem_access_ptr->getDestUnit();
        const L23UnitName &src_unit  = mem_access_ptr->getSrcUnit();
        
        const MemAccess::RequestType &req_type  = mem_access_ptr->getReqType();

        if (src_unit == L23UnitName::LSU || src_unit == L23UnitName::ICACHE) {

            // This is L2 responding
            if (dest_unit == L23UnitName::LSU) {
                appendLSUReadRespQueue_(mem_access_ptr);
            }
            else if (dest_unit == L23UnitName::ICACHE) {
                appendICacheRespQueue_(mem_access_ptr);
            }
            else if (dest_unit == L23UnitName::L2CACHE) {
                DLOG("Resp is not required to be sent for the write request.");
            }
            else if (dest_unit == L23UnitName::L3CACHE) {
                DLOG("Resp is not required to be sent for the L3-PF request.");
            }
            else {
                sparta_assert(false, "Incorrect Destination Unit");
            }
        }
        else if (src_unit == L23UnitName::L2CACHE) {
            
            // This is L3 responding
            if (dest_unit == L23UnitName::L2CACHE ||
                dest_unit == L23UnitName::ICACHE  ||
                dest_unit == L23UnitName::LSU) {
                appendRWRespQueue_(mem_access_ptr);
            }
            else if (dest_unit == L23UnitName::L3CACHE) {
                DLOG("Resp is not required to be sent for the write request.");
            }
            else {
                sparta_assert(false, "Incorrect Destination Unit");
            }
        }
        else {
            sparta_assert(false, "Resp is being sent to a Unit that is not valid");
        }

        // Send out the prefetch completion ack
        if (req_type == MemAccess::RequestType::PREFETCH) {
            if ((src_unit == L23UnitName::LSU     && dest_unit == L23UnitName::L2CACHE) || 
                (src_unit == L23UnitName::L2CACHE && dest_unit == L23UnitName::L3CACHE)) {
                
                // Only send ack when completing the Prefetch Request at this Unit
                appendPFAckQueue_(mem_access_ptr);
            }
        }
    }

    // Send the request to the slave units
    void L23::sendOutReq_(const MemAccessPtr& mem_access_ptr) {

        const L23UnitName& src_unit = mem_access_ptr->getSrcUnit();

        // if (mem_access_ptr is destined for BIU on L23 miss or L23 eviction)
        if (src_unit == L23UnitName::L2CACHE || 
            src_unit == L23UnitName::L3CACHE) {
            appendBIUReqQueue_(mem_access_ptr);
        }
        else {
            sparta_assert(false, "Request is being sent by a Unit that is not valid");
        }
    }

    // Check if any input request is available to be scheduled on the L23 Banks
    bool L23::isReqAvailableforBanks() {

        bool req_available = true;

        if (  icache_req_queue_.size() == 0
           && rw_req_queue_.size() == 0
           && lsu_read_req_queue_.size() == 0
           && lsu_write_req_queue_.size() == 0
           && !is_MRQ_Ready_()) {

            req_available = false;
        }

        return req_available;
    }

    // Check if this MRQ has valid entry whose data is available
    bool L23::is_MRQ_Ready_(const uint32_t& bank_id, const uint32_t& row_id) {
        
        const auto& q0 = l23_bank_[bank_id]->miss_pending_buffer_[row_id];
        DLOG("miss_pending_buffer_" << " BANK ID: " << bank_id << " ROW ID: " << row_id << " Entries remaining: " << q0->size());
        if (q0->size() > 0) {
            for (const auto& i: (*q0)) {
                if (i->getL23MrqState() == MrqState::READY) {
                    ILOG("miss_pending_buffer_ has an entry that is READY" << i);
                    return true;
                }
            }
            return false;
        }
        else {
            return false;
        }
    }

    // Check if any MRQ has valid entry whose data is available
    bool L23::is_MRQ_Ready_() {
        for (uint32_t bank_select = 0; bank_select < num_banks_; ++bank_select) {
            for (uint32_t row_select = 0; row_select < num_rows_per_bank_; ++row_select) {
            
                if (is_MRQ_Ready_(bank_select, row_select)) {
                    return true;
                }
            }
        }
        return false;
    }

    // Select the channel to pick the request from
    // Current options :
    //       BIU       - P0
    //       ICache    - P1 - RoundRobin Candidate
    //       LSU_Read  - P1 - RoundRobin Candidate
    //       LSU_Write - P1 - RoundRobin Candidate
    L23::Channel L23::arbitrateL23AccessReqs_(const uint32_t& bank_id, const uint32_t& row_id) {

        Channel winner = Channel::NO_ACCESS;

        if (is_MRQ_Ready_(bank_id, row_id)) {

            if (l23_bank_[bank_id]->biu_pipe_req_queue_[row_id]->numFree() == 0) {
                DLOG("biu_pipe_req_queue_ is full for bank_id : " << bank_id
                                                    << " row_id : " << row_id);
            }
            else {
                winner = Channel::BIU;
            }
        }

        if (!rw_req_queue_.empty() && winner == Channel::NO_ACCESS) {

            if (bank_id == get_BankID_(rw_req_queue_.front())
             && row_id  == get_RowID_(rw_req_queue_.front())) {

                // Row and bank should match
                winner = Channel::READWRITE;
            }
        }

        if (!icache_req_queue_.empty() && winner == Channel::NO_ACCESS) {

            if (bank_id == get_BankID_(icache_req_queue_.front())
             && row_id  == get_RowID_(icache_req_queue_.front())) {

                // Row and bank should match
                winner = Channel::ICACHE;
            }
        }

        if (!lsu_read_req_queue_.empty() && winner == Channel::NO_ACCESS) {

            if (bank_id == get_BankID_(lsu_read_req_queue_.front())
             && row_id  == get_RowID_(lsu_read_req_queue_.front())) {

                // Row and bank should match
                winner = Channel::LSU_READ;
            }
        }

        if (!lsu_write_req_queue_.empty() && winner == Channel::NO_ACCESS) {

            if (bank_id == get_BankID_(lsu_write_req_queue_.front())
             && row_id  == get_RowID_(lsu_write_req_queue_.front())) {

                // Row and bank should match
                winner = Channel::LSU_WRITE;
            }
        }

        return winner;
    }

    // Allocating the cacheline in the L23 based on return from BIU/L3 or stat prefetcher
    void L23::reloadL23Cache_(const MemAccessPtr& ma_ptr) {
        auto req_src_unit = ma_ptr->getSrcUnit();
        auto [is_valid, is_dirty, evicted_cl_addr] = cache_->reload_mem_access(ma_ptr);

        // Check if eviction happened with this Reload
        MemAccessPtr evict_ma_ptr = sparta::allocate_sparta_shared_pointer<MemAccess> (memory_access_allocator_, evicted_cl_addr);
        
        if (req_src_unit == MemAccess::UnitName::LSU || req_src_unit == MemAccess::UnitName::ICACHE) {
            evict_ma_ptr->setSrcUnit(MemAccess::UnitName::L2CACHE);
            evict_ma_ptr->setDestUnit(MemAccess::UnitName::L3CACHE);
        }
        else if (req_src_unit == MemAccess::UnitName::L2CACHE) {
            evict_ma_ptr->setSrcUnit(MemAccess::UnitName::L3CACHE);
            evict_ma_ptr->setDestUnit(MemAccess::UnitName::BIU);
        }
        evict_ma_ptr->setDataReady(true);
        evict_ma_ptr->setReqType(MemAccess::RequestType::WRITE);
        evict_ma_ptr->setModified(is_dirty);
        
        if (is_valid && (is_dirty || evict_clean_cl_)) {

            ILOG("L23Cache Eviction: " << evict_ma_ptr);
            sendOutReq_(evict_ma_ptr);

            if (is_dirty) ++dirty_evictions_;
            else ++clean_evictions_;
        }
        else if (is_valid) {

            ILOG("L23Cache Dropped Eviction : " << evict_ma_ptr);
            ++dropped_evictions_;
        }
    }

    // Check if there are enough credits for the request to be issued to the l23_pipeline_
    bool L23Bank::hasCreditsForPipelineIssue_(const uint32_t& row_id) {

        uint32_t num_free_biu_req_queue = l23_->biu_req_queue_size_ - l23_->biu_req_queue_.size();
        uint32_t num_free_miss_pending_buffer = miss_pending_buffer_[row_id]->numFree();

        uint32_t num_free_pfack_resp_queue = l23_->pfack_resp_queue_size_ - l23_->pfack_resp_queue_.size();

        uint32_t empty_slots = std::min(num_free_biu_req_queue, num_free_miss_pending_buffer);
                 empty_slots = std::min(empty_slots, num_free_pfack_resp_queue);
        
        DLOG("BIU slots: " << num_free_biu_req_queue << " - MSHR slots: " << num_free_miss_pending_buffer);
        DLOG("PFACK Resp slots: " << num_free_pfack_resp_queue);
        DLOG("Inflight req : " << l23_->inFlight_reqs_ << " - Empty slots : " << empty_slots);
        
        return (l23_->inFlight_reqs_ < empty_slots);
    }

    void L23Bank::update_Bank_Scheduling_Delay_() {
        --bank_scheduling_delay_counter_;

        if (bank_scheduling_delay_counter_ != 0) {
            uev_update_bank_scheduling_delay_->schedule(sparta::Clock::Cycle(1));
        }
    }

}