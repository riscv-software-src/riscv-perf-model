// ICacheSink.hpp
#pragma once

#include "core/MemoryAccessInfo.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/LogUtils.hpp"


namespace icache_test
{
    class ICacheSink : public sparta::Unit
    {
    public:
        static constexpr char name[] = "icache_sink_unit";

        class ICacheSinkParameters : public sparta::ParameterSet
        {
        public:
            ICacheSinkParameters(sparta::TreeNode *n) : sparta::ParameterSet(n)
            {}
            PARAMETER(double,   miss_rate,      0, "miss rate per 1k requests")
            PARAMETER(uint32_t, latency,        8, "hit latency")
            PARAMETER(uint32_t, miss_penalty,  32, "miss latency")

        };

        struct ICacheResponse {
            uint64_t                              scheduled_time;
            olympia::MemoryAccessInfo::CacheState hit_state;
            olympia::MemoryAccessInfoPtr          access;
        };


        ICacheSink(sparta::TreeNode *n, const ICacheSinkParameters *params) :
            sparta::Unit(n),
            latency_(params->latency),
            miss_penalty_(params->miss_penalty),
            miss_distribution_({1000, params->miss_rate}),
            gen_(1)
        {
            in_icache_req_.registerConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(ICacheSink, getRequestFromICache_, olympia::MemoryAccessInfoPtr));

            sparta::StartupEvent(n, CREATE_SPARTA_HANDLER(ICacheSink, sendInitialCredits_));
            ev_respond_.setContinuing(true);

        }

        void setRandomSeed(uint32_t seed)
        {
            gen_.seed(seed);
        }


    private:

        ////////////////////////////////////////////////////////////////////////////////
        // Methods
        ////////////////////////////////////////////////////////////////////////////////

        void sendInitialCredits_()
        {
            out_icache_credit_.send(8);
        }

        void getRequestFromICache_(const olympia::MemoryAccessInfoPtr & mem_access_info_ptr)
        {
            // Hit's are removed from the pending queue
            // Misses are retained.
            // Randomly choose to miss, and also randomly generate a latency
            ILOG("received request " << mem_access_info_ptr);
            ICacheResponse resp;
            resp.access = mem_access_info_ptr;
            resp.scheduled_time = getClock()->currentCycle() + latency_;
            if (miss_distribution_(gen_)) {
                resp.hit_state = olympia::MemoryAccessInfo::CacheState::MISS;
            }
            else {
                resp.hit_state = olympia::MemoryAccessInfo::CacheState::HIT;
            }
            response_queue_.emplace_back(resp);
            // mem_access_info_ptr->setCacheState(olympia::MemoryAccessInfo::CacheState::HIT);
            // ev_respond_.preparePayload(mem_access_info_ptr)->schedule(10);
            ev_respond_.schedule(latency_);
            out_icache_credit_.send(1);
        }

        void sendResponse_()
        {
            // Find first response that's ready
            auto response_ready = [this](ICacheResponse resp){
                return resp.scheduled_time <= getClock()->currentCycle();
            };
            auto resp_iter = std::find_if(response_queue_.begin(), response_queue_.end(), response_ready);
            if (resp_iter == response_queue_.end()) {
                ev_respond_.schedule(1);
                return;
            }

            ILOG("sending response " << resp_iter->access);
            out_icache_resp_.send(resp_iter->access);

            // Replay the miss later
            resp_iter->access->setCacheState(resp_iter->hit_state);
            if (resp_iter->hit_state == olympia::MemoryAccessInfo::CacheState::MISS) {
                resp_iter->hit_state = olympia::MemoryAccessInfo::CacheState::HIT;
                resp_iter->scheduled_time = getClock()->currentCycle() + miss_penalty_;
                ev_respond_.schedule(1);
            }
            else {
                response_queue_.erase(resp_iter);
            }
        }

        void onStartingTeardown_() override final
        {
            sparta_assert(response_queue_.empty());
        }


        ////////////////////////////////////////////////////////////////////////////////
        // Variables
        ////////////////////////////////////////////////////////////////////////////////

        const uint32_t latency_;
        const uint32_t miss_penalty_;

        std::vector<ICacheResponse> response_queue_;

        std::discrete_distribution<> miss_distribution_;
        std::mt19937 gen_;

        ////////////////////////////////////////////////////////////////////////////////
        // Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataInPort<olympia::MemoryAccessInfoPtr> in_icache_req_{&unit_port_set_,
                                                             "in_icache_req", 0};

        sparta::DataOutPort<olympia::MemoryAccessInfoPtr> out_icache_resp_{&unit_port_set_,
                                                             "out_icache_resp", 0};

        sparta::DataOutPort<uint32_t> out_icache_credit_{&unit_port_set_,
                                                       "out_icache_credit", 0};

        ////////////////////////////////////////////////////////////////////////////////
        // Events
        ////////////////////////////////////////////////////////////////////////////////
        sparta::UniqueEvent<> ev_respond_{
            &unit_event_set_, "ev_respond",
            CREATE_SPARTA_HANDLER(ICacheSink, sendResponse_)};
    };
}