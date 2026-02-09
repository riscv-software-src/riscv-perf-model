
#pragma once

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/LogUtils.hpp"

#include <string>

namespace l2cache_test
{
    // ////////////////////////////////////////////////////////////////////////////////
    // "Sink" unit, just sinks instructions sent to it.  Sends credits
    // back as directed by params/execution mode
    class BIUSinkUnit : public sparta::Unit
    {
        public:
            static constexpr char name[] = "BIUSinkUnit";

            class BIUSinkUnitParameters : public sparta::ParameterSet
            {
            public:
                explicit BIUSinkUnitParameters(sparta::TreeNode *n) :
                    sparta::ParameterSet(n)
                { }
                PARAMETER(std::string, purpose, "grp", "Purpose of this SinkUnit: grp, single")
                PARAMETER(sparta::Clock::Cycle, sink_latency, 10, "Latency of this SinkUnit")
            };

            BIUSinkUnit(sparta::TreeNode * n, const BIUSinkUnitParameters * params) :  sparta::Unit(n) {

                purpose_ = params->purpose;
                sink_latency_ = params->sink_latency;

                in_biu_req_.registerConsumerHandler
                        (CREATE_SPARTA_HANDLER_WITH_DATA(BIUSinkUnit, sinkInst_, olympia::MemoryAccessInfoPtr));

                sparta::StartupEvent(n, CREATE_SPARTA_HANDLER(BIUSinkUnit, sendInitialCredits_));
            }
        private:

            // Sending Initial credits to L2Cache
            void sendInitialCredits_() {
                uint32_t biu_req_queue_size_ = 32;
                out_biu_credits_.send(biu_req_queue_size_);
                ILOG("Sending initial credits to L2Cache : " << biu_req_queue_size_);
            }

            void sinkInst_(const olympia::MemoryAccessInfoPtr & mem_access_info_ptr) {
                ILOG("Instruction: '" << mem_access_info_ptr->getInstPtr() << "' sinked");

                out_biu_credits_.send(1, sink_latency_);
                out_biu_resp_.send(mem_access_info_ptr, 2*sink_latency_);
            }

            sparta::DataInPort<olympia::MemoryAccessInfoPtr>  in_biu_req_     {&unit_port_set_, "in_biu_req",
                                                                               sparta::SchedulingPhase::Tick, 1};
            sparta::DataOutPort<olympia::MemoryAccessInfoPtr> out_biu_resp_ {&unit_port_set_, "out_biu_resp"};
            sparta::DataOutPort<uint32_t>                     out_biu_credits_ {&unit_port_set_, "out_biu_credits"};

            std::string purpose_;
            sparta::Clock::Cycle sink_latency_;
    };
}
