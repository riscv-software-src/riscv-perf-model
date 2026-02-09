// Source for ICache
#pragma once

#include "core/MemoryAccessInfo.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/LogUtils.hpp"

#include "OlympiaAllocators.hpp"

namespace icache_test
{
    class ICacheSource : public sparta::Unit
    {
    public:
        static constexpr char name[] = "icache_source_unit";

        class ICacheSourceParameters : public sparta::ParameterSet
        {
        public:
            ICacheSourceParameters(sparta::TreeNode *n) : sparta::ParameterSet(n)
            {}
        };

        ICacheSource(sparta::TreeNode *n, const ICacheSourceParameters *params) :
            sparta::Unit(n),
            memory_access_allocator_(
                sparta::notNull(olympia::OlympiaAllocators::getOlympiaAllocators(n))->
                    memory_access_allocator)
        {
            in_icache_resp_.registerConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(ICacheSource, getResponseFromICache_, olympia::MemoryAccessInfoPtr));
            in_icache_credit_.registerConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(ICacheSource, getCreditFromICache_, uint32_t));
        }

        // Queue up an ICache request for the address given
        void queueRequest(const uint64_t & addr)
        {
            request_queue_.emplace_back(addr);
            ev_send_requests_.schedule(sparta::Clock::Cycle(1));
        }


    private:

        ////////////////////////////////////////////////////////////////////////////////
        // Methods
        ////////////////////////////////////////////////////////////////////////////////

        void sendRequests_()
        {
            if (!request_queue_.empty()) {
                if (icache_credits_ > 0) {
                    auto addr = request_queue_.front();
                    auto memory_access_info_ptr =
                        sparta::allocate_sparta_shared_pointer<olympia::MemoryAccessInfo>(
                            memory_access_allocator_, addr);
                    ILOG("sending " << memory_access_info_ptr);
                    out_icache_req_.send(memory_access_info_ptr);
                    icache_credits_--;
                    request_queue_.pop_front();
                    ++outstanding_reqs_;
                }
                ev_send_requests_.schedule(1);
            }
        }

        void getResponseFromICache_(const olympia::MemoryAccessInfoPtr & mem_access_info_ptr)
        {
            // Hit's are removed from the pending queue
            // Misses are retained.
            ILOG("received response " << mem_access_info_ptr);
            if (mem_access_info_ptr->getCacheState() == olympia::MemoryAccessInfo::CacheState::HIT) {
                --outstanding_reqs_;
            }
        }

        void getCreditFromICache_(const uint32_t & credits)
        {
            icache_credits_ += credits;
        }

        void onStartingTeardown_() override final
        {
            sparta_assert(outstanding_reqs_ == 0);
        }


        ////////////////////////////////////////////////////////////////////////////////
        // Variables
        ////////////////////////////////////////////////////////////////////////////////

        olympia::MemoryAccessInfoAllocator & memory_access_allocator_;
        uint32_t icache_credits_ = 0;
        std::deque<uint64_t> request_queue_;

        uint32_t outstanding_reqs_ = 0;

        ////////////////////////////////////////////////////////////////////////////////
        // Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataOutPort<olympia::MemoryAccessInfoPtr> out_icache_req_{&unit_port_set_,
                                                             "out_icache_req", 0};

        sparta::DataInPort<olympia::MemoryAccessInfoPtr> in_icache_resp_{&unit_port_set_,
                                                             "in_icache_resp", 0};

        sparta::DataInPort<uint32_t> in_icache_credit_{&unit_port_set_,
                                                       "in_icache_credit", 0};

        ////////////////////////////////////////////////////////////////////////////////
        // Events
        ////////////////////////////////////////////////////////////////////////////////
        sparta::UniqueEvent<> ev_send_requests_{&unit_event_set_, "ev_send_requests",
            CREATE_SPARTA_HANDLER(ICacheSource, sendRequests_)};

    };
}