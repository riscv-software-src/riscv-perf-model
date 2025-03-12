#pragma once

#include <sparta/ports/DataPort.hpp>
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "core/MemoryAccessInfo.hpp"
#include "core/decode/MavisUnit.hpp"

namespace dcache_test
{
    class NextLvlSourceSinkUnit : public sparta::Unit
    {
      public:
        static constexpr char name[] = "NextLvlSourceSinkUnit";

        class NextLvlSinkUnitParameters : public sparta::ParameterSet
        {
          public:
            explicit NextLvlSinkUnitParameters(sparta::TreeNode* n) : sparta::ParameterSet(n) {}
            PARAMETER(std::string, purpose, "grp", "Purpose of this SinkUnit: grp, single")
            PARAMETER(sparta::Clock::Cycle, sink_latency, 1, "Latency of this SinkUnit")
        };

        NextLvlSourceSinkUnit(sparta::TreeNode* n, const NextLvlSinkUnitParameters* params) :
            sparta::Unit(n)
        {

            purpose_ = params->purpose;
            sink_latency_ = params->sink_latency;

            in_biu_req_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(
                NextLvlSourceSinkUnit, sinkInst_, olympia::MemoryAccessInfoPtr));
        }

      private:
        void sinkInst_(const olympia::MemoryAccessInfoPtr & mem_access_info_ptr)
        {
            ILOG("Instruction: '" << mem_access_info_ptr->getInstPtr() << "' sinked");

            out_biu_resp_.send(mem_access_info_ptr, 2 * sink_latency_);
        }

        sparta::DataInPort<olympia::MemoryAccessInfoPtr> in_biu_req_{
            &unit_port_set_, "in_biu_req", sparta::SchedulingPhase::Tick, 1};
        sparta::DataOutPort<olympia::MemoryAccessInfoPtr> out_biu_resp_{&unit_port_set_,
                                                                        "out_biu_resp"};
        sparta::DataOutPort<uint32_t> out_biu_ack_{&unit_port_set_, "out_biu_ack"};

        std::string purpose_;
        sparta::Clock::Cycle sink_latency_;
    };
} // namespace dcache_test
