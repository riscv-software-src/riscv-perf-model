#pragma once


#include "core/MemoryAccessInfo.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "ICache.hpp"

#include <string>
#include <map>
#include <set>

// Basically just a way of getting to the address decoder so that the checker can
// calculate set index or block addresses
class olympia::ICacheTester
{
public:
    void setDUT(olympia::ICache *dut) { dut_ = dut; }

    uint64_t getSetIdx(uint64_t addr)
    {
        auto decoder = dut_->l1_cache_->getAddrDecoder();
        return decoder->calcIdx(addr);
    }

    uint64_t getTag(uint64_t addr)
    {
        auto decoder = dut_->l1_cache_->getAddrDecoder();
        return decoder->calcTag(addr);
    }

    uint64_t getBlockAddress(uint64_t addr)
    {
        auto decoder = dut_->l1_cache_->getAddrDecoder();
        return decoder->calcBlockAddr(addr);
    }

    uint32_t getNumWays()
    {
        return dut_->l1_cache_->getNumWays();
    }

private:
    olympia::ICache * dut_ = nullptr;
};

namespace icache_test
{
    class ICacheChecker : public sparta::Unit, public olympia::ICacheTester
    {
    public:
        static constexpr char name[] = "instruction cache checker";

        using ICacheCheckerParameters = sparta::ParameterSet;
        ICacheChecker(sparta::TreeNode * node, const ICacheCheckerParameters *p) : sparta::Unit(node)
        {

            in_fetch_req_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(ICacheChecker, getRequestFromFetch_, olympia::MemoryAccessInfoPtr));
            in_fetch_resp_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(ICacheChecker, getResponseToFetch_, olympia::MemoryAccessInfoPtr));
            in_l2cache_req_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(ICacheChecker, getRequestToL2Cache_, olympia::MemoryAccessInfoPtr));
            in_l2cache_resp_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(ICacheChecker, getResponseFromL2Cache_, olympia::MemoryAccessInfoPtr));
        }

        uint32_t getICacheHitCount() const  { return icache_hits_;}
        uint32_t getICacheMissCount() const { return icache_misses_;}
        uint32_t getL2CacheHitCount() const  { return l2cache_hits_;}
        uint32_t getL2CacheMissCount() const { return l2cache_misses_;}

    private:

        void getRequestFromFetch_(const olympia::MemoryAccessInfoPtr & mem_access_info_ptr)
        {
            fetch_pending_queue_.push_back(mem_access_info_ptr);
        }

        void getResponseToFetch_(const olympia::MemoryAccessInfoPtr & mem_access_info_ptr)
        {
            // Should only be a HIT or MISS response
            const auto cache_state = mem_access_info_ptr->getCacheState();
            if (cache_state != olympia::MemoryAccessInfo::CacheState::HIT) {
                sparta_assert(cache_state == olympia::MemoryAccessInfo::CacheState::MISS);
            }

            // Search for the original request
            const auto fetch_req = std::find(fetch_pending_queue_.begin(), fetch_pending_queue_.end(), mem_access_info_ptr);
            sparta_assert(fetch_req != fetch_pending_queue_.end(), "response received without a corresponding request");

            auto tag = getTag(mem_access_info_ptr->getPAddr());
            auto set = getSetIdx(mem_access_info_ptr->getPAddr());

            if (cache_state == olympia::MemoryAccessInfo::CacheState::HIT) {
                auto block = getBlockAddress(mem_access_info_ptr->getPAddr());

                // Check that we don't have an outstanding L2 request on this block
                sparta_assert(pending_l2cache_reqs_.count(block) == 0);

                // Check that we've filled this block at least once..
                sparta_assert(filled_blocks_.count(block));

                // Track the last tag to hit on this set
                last_access_tracker_[set] = tag;

                ILOG("removing fetch request")
                fetch_pending_queue_.erase(fetch_req);

                ++icache_hits_;
            }
            else {
                // We cannot miss if we hit on this set last time with this tag
                if (auto itr = last_access_tracker_.find(set); itr != last_access_tracker_.end()) {
                    sparta_assert(itr->second != tag);
                }

                ++icache_misses_;
            }
        }

        void getRequestToL2Cache_(const olympia::MemoryAccessInfoPtr & mem_access_info_ptr)
        {

            auto block = getBlockAddress(mem_access_info_ptr->getPAddr());
            auto matches_block = [this, block](auto req) { return block == getBlockAddress(req->getPAddr()); };

            // Check that fetch has tried to request this address
            const auto fetch_req = std::find_if(fetch_pending_queue_.begin(), fetch_pending_queue_.end(), matches_block);
            sparta_assert(fetch_req != fetch_pending_queue_.end(), "response received without a corresponding request");

            // Check that we don't have another l2cache request inflight for the same block
            sparta_assert(pending_l2cache_reqs_.count(block) == 0);
            pending_l2cache_reqs_.insert(block);
        }

        void getResponseFromL2Cache_(const olympia::MemoryAccessInfoPtr & mem_access_info_ptr)
        {
            if (mem_access_info_ptr->getCacheState() == olympia::MemoryAccessInfo::CacheState::HIT) {
                auto block = getBlockAddress(mem_access_info_ptr->getPAddr());

                // Flag that we've filled this block atleast once
                filled_blocks_.insert(block);

                // Shouldn't have received a response if we didn't request it?
                sparta_assert(pending_l2cache_reqs_.erase(block));

                ++l2cache_hits_;
            }
            else {
                ++l2cache_misses_;
            }
        }

        void onStartingTeardown_() override final
        {
            sparta_assert(fetch_pending_queue_.empty());
            sparta_assert(pending_l2cache_reqs_.size() == 0);
        }

        ////////////////////////////////////////////////////////////////////////////////
        // Variables
        ////////////////////////////////////////////////////////////////////////////////

        uint32_t icache_hits_ = 0;
        uint32_t icache_misses_ = 0;
        uint32_t l2cache_hits_ = 0;
        uint32_t l2cache_misses_ = 0;

        // Track pending fetch requests
        std::vector<olympia::MemoryAccessInfoPtr> fetch_pending_queue_;

        // Track pending l2cache reqs - icache shouldn't request them again, and fetch cannot hit on them
        std::set<uint64_t> pending_l2cache_reqs_;

        // Track blocks which have been filled atleast once - fetch cannot hit on them until they're filled
        std::set<uint64_t> filled_blocks_;

        // Track the tags of the last hits on each set - fetch cannot miss on them
        std::map<uint32_t, uint64_t> last_access_tracker_;

        ////////////////////////////////////////////////////////////////////////////////
        // Input Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataInPort<olympia::MemoryAccessInfoPtr> in_fetch_req_    {&unit_port_set_, "in_fetch_req",    1};
        sparta::DataInPort<olympia::MemoryAccessInfoPtr> in_fetch_resp_   {&unit_port_set_, "in_fetch_resp",   1};
        sparta::DataInPort<olympia::MemoryAccessInfoPtr> in_l2cache_req_  {&unit_port_set_, "in_l2cache_req",  1};
        sparta::DataInPort<olympia::MemoryAccessInfoPtr> in_l2cache_resp_ {&unit_port_set_, "in_l2cache_resp", 1};
    };
}