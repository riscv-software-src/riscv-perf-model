// <ICache.cpp> -*- C++ -*-

//!
//! \file ICache.cpp
//! \brief Implementation of the CoreModel ICache unit
//!


#include "ICache.hpp"

#include "OlympiaAllocators.hpp"


namespace olympia {
    const char ICache::name[] = "icache";

    ICache::ICache(sparta::TreeNode *node, const ICacheParameterSet *p) :
        sparta::Unit(node),
        l1_always_hit_(p->l1_always_hit),
        cache_latency_(p->cache_latency),
        pending_miss_buffer_("pending_miss_buffer", fetch_queue_size_, getClock()),
        memory_access_allocator_(
            sparta::notNull(olympia::OlympiaAllocators::getOlympiaAllocators(node))->
                memory_access_allocator)
    {

        in_fetch_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(ICache, getRequestFromFetch_, MemoryAccessInfoPtr));

        in_l2cache_credits_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(ICache, getCreditsFromL2Cache_, uint32_t));

        in_l2cache_resp_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(ICache, getRespFromL2Cache_, MemoryAccessInfoPtr));

        // IL1 cache config
        const uint32_t l1_line_size = p->l1_line_size;
        const uint32_t l1_size_kb = p->l1_size_kb;
        const uint32_t l1_associativity = p->l1_associativity;
        const std::string& replacement_policy = p->l1_replacement_policy;
        l1_cache_.reset(new CacheFuncModel(getContainer(), l1_size_kb, l1_line_size, replacement_policy, l1_associativity));

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(ICache, sendInitialCredits_));
    }

    void ICache::sendInitialCredits_()
    {
        out_fetch_credit_.send(fetch_queue_size_);
    }

    // Access ICache
    bool ICache::lookupCache_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        uint64_t phyAddr = mem_access_info_ptr->getPhyAddr();

        bool cache_hit = false;

        if (l1_always_hit_) {
            cache_hit = true;
        }
        else {
            auto cache_line = l1_cache_->peekLine(phyAddr);
            cache_hit = (cache_line != nullptr) && cache_line->isValid();

            // Update MRU replacement state if ICache HIT
            if (cache_hit) {
                l1_cache_->touchMRU(*cache_line);
            }
        }

        if (l1_always_hit_) {
            ILOG("IL1 Cache HIT all the time: phyAddr=0x" << std::hex << phyAddr);
            il1_cache_hits_++;
        }
        else if (cache_hit) {
            ILOG("IL1 Cache HIT: phyAddr=0x" << std::hex << phyAddr);
            il1_cache_hits_++;
        }
        else {
            ILOG("IL1 Cache MISS: phyAddr=0x" << std::hex << phyAddr);
            il1_cache_misses_++;
        }

        return cache_hit;
    }

    void ICache::reloadCache_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {

        auto const decoder = l1_cache_->getAddrDecoder();
        auto const reload_addr = mem_access_info_ptr->getPhyAddr();
        auto const reload_block = decoder->calcBlockAddr(reload_addr);

        auto l1_cache_line = &l1_cache_->getLineForReplacementWithInvalidCheck(reload_addr);
        l1_cache_->allocateWithMRUUpdate(*l1_cache_line, reload_addr);

        // Move pending misses into the replay queue
        DLOG("finding misses to replay");
        auto iter = pending_miss_buffer_.begin();
        while (iter != pending_miss_buffer_.end()) {
            auto delete_iter = iter++;

            if (decoder->calcBlockAddr((*delete_iter)->getPhyAddr()) == reload_block) {
                DLOG("scheduling for replay " << *delete_iter);
                replay_buffer_.emplace_back(*delete_iter);
                pending_miss_buffer_.erase(delete_iter);
            }
        }

        // Schedule next cycle
        DLOG("reload completed");
        ev_arbitrate_.schedule(1);
    }

    void ICache::doArbitration_()
    {
        if (!l2cache_resp_queue_.empty()) {
            // Do a linefill
            auto const mem_access_info_ptr = l2cache_resp_queue_.front();
            ILOG("doing reload " << mem_access_info_ptr);
            reloadCache_(mem_access_info_ptr);
            l2cache_resp_queue_.pop_front();
        }

        // Priotize replays over fetches, replays can run in parallel with a fill.
        // NOTE: Ideally we'd want to prioritize demand fetches over lingering misses
        // from a speculative search
        if (!replay_buffer_.empty()) {
            // Replay miss
            auto const mem_access_info_ptr = replay_buffer_.front();
            ILOG("doing replay for fetch request " << mem_access_info_ptr);
            ev_replay_ready_.preparePayload(mem_access_info_ptr)
                ->schedule(sparta::Clock::Cycle(cache_latency_));
            replay_buffer_.pop_front();

        }
        else if (!fetch_req_queue_.empty()) {
            // Do a read access
            auto const mem_access_info_ptr = fetch_req_queue_.front();
            ILOG("doing lookup for fetch request " << mem_access_info_ptr);
            if (lookupCache_(mem_access_info_ptr)) {
                mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
            }
            else {
                mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);
                addToMissQueue_(mem_access_info_ptr);
            }
            ev_respond_.preparePayload(mem_access_info_ptr)
                ->schedule(sparta::Clock::Cycle(cache_latency_));
            fetch_req_queue_.pop_front();
        }

        if (!l2cache_resp_queue_.empty() || !replay_buffer_.empty() || !fetch_req_queue_.empty()) {
            ev_arbitrate_.schedule(1);
        }
    }

    void ICache::addToMissQueue_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        // Don't make requests to cachelines that are already pending
        auto const decoder = l1_cache_->getAddrDecoder();
        auto missed_block = decoder->calcBlockAddr(mem_access_info_ptr->getPhyAddr());
        auto same_line = [decoder, missed_block] (auto other) {
            return decoder->calcBlockAddr(other->getPhyAddr()) == missed_block;
        };
        auto it = std::find_if(pending_miss_buffer_.begin(), pending_miss_buffer_.end(), same_line);
        if (it == pending_miss_buffer_.end()) {
            DLOG("appending miss to l2 miss queue: " << mem_access_info_ptr);
            miss_queue_.emplace_back(mem_access_info_ptr);
            makeL2CacheRequest_();
        }
        ILOG("miss request queued for replay: " << mem_access_info_ptr);
        pending_miss_buffer_.push_back(mem_access_info_ptr);
    }

    void ICache::getRequestFromFetch_(const MemoryAccessInfoPtr &mem_access_info_ptr)
    {
        ILOG("received fetch request " << mem_access_info_ptr);
        fetch_req_queue_.emplace_back(mem_access_info_ptr);
        ev_arbitrate_.schedule(sparta::Clock::Cycle(0));
    }

    void ICache::getRespFromL2Cache_(const MemoryAccessInfoPtr &mem_access_info_ptr)
    {
        ILOG("received fill response " << mem_access_info_ptr);
        if (mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT) {
            l2cache_resp_queue_.emplace_back(mem_access_info_ptr);
            ev_arbitrate_.schedule(sparta::Clock::Cycle(0));
        }
    }

    void ICache::getCreditsFromL2Cache_(const uint32_t &ack)
    {
        l2cache_credits_ += ack;
        if (!miss_queue_.empty()) {
            ev_l2cache_request_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Respond misses
    void ICache::sendReplay_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        // Delayed change to hit state until we're ready to send it back
        mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
        out_fetch_resp_.send(mem_access_info_ptr);
        out_fetch_credit_.send(1);
    }

    void ICache::sendResponse_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        out_fetch_resp_.send(mem_access_info_ptr);
        if (mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT) {
            out_fetch_credit_.send(1);
        }
    }

    void ICache::makeL2CacheRequest_()
    {
        if (l2cache_credits_ == 0 || miss_queue_.empty()) {
            return;
        }

        // Create new MemoryAccessInfo to avoid propagating changes made by L2 back to the core
        const auto &l2cache_req = sparta::allocate_sparta_shared_pointer<MemoryAccessInfo>(
            memory_access_allocator_, *(miss_queue_.front()));

        // Forward miss to next cache level
        ILOG("requesting linefill for " << l2cache_req);
        out_l2cache_req_.send(l2cache_req);
        --l2cache_credits_;

        miss_queue_.pop_front();

        // Schedule another
        if (l2cache_credits_ > 0 && !miss_queue_.empty()) {
            ev_l2cache_request_.schedule(1);
        }
    }
}
