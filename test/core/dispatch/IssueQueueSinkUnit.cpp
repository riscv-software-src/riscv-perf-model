
#pragma once

#include "core/InstGenerator.hpp"

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/LogUtils.hpp"

#include "test/core/common/SinkUnit.hpp"

#include <string>

namespace rename_test
{
    // ////////////////////////////////////////////////////////////////////////////////
    // "Sink" unit, just sinks instructions sent to it.  Sends credits
    // back as directed by params/execution mode
    class IssueQueueSinkUnit : public core_test::SinkUnit
    {
        public:
            static constexpr char name[] = "IssueQueueSinkUnit";

            IssueQueueSinkUnit(sparta::TreeNode * n, const SinkUnitParameters * params) : SinkUnit(n, params)
            {
                if(params->purpose == "grp") {
                    in_sink_retire_inst_.registerConsumerHandler
                        (CREATE_SPARTA_HANDLER_WITH_DATA(IssueQueueSinkUnit, sinkRetireInst_<olympia::InstGroupPtr>, olympia::InstGroupPtr));
                }
                else {
                    in_sink_retire_inst_.registerConsumerHandler
                        (CREATE_SPARTA_HANDLER_WITH_DATA(IssueQueueSinkUnit, sinkRetireInst_<olympia::InstPtr>, olympia::InstPtr));
                }
            }
        private:
            template<class InstType>
            void sinkRetireInst_(const InstType & inst_or_insts)
            {
                if constexpr(std::is_same_v<InstType, olympia::InstGroupPtr>)
                {
                    for(auto ptr : *inst_or_insts) {
                        ILOG("Instruction: '" << ptr << "' sinked");
                        auto & ex_inst = *ptr;
                        ex_inst.setStatus(olympia::Inst::Status::RETIRED);
                        out_rob_retire_ack_.send(ptr);
                    }
                }
                else {
                    ILOG("Instruction: '" << inst_or_insts << "' sinked");
                    auto & ex_inst = *inst_or_insts;
                    ex_inst.setStatus(olympia::Inst::Status::RETIRED);
                    out_rob_retire_ack_.send(inst_or_insts);
                }
            }

            sparta::DataInPort<olympia::InstPtr>      in_sink_retire_inst_     {&unit_port_set_, "in_sink_retire_inst",
                                                                                sparta::SchedulingPhase::Tick, 1};
            sparta::DataInPort<olympia::InstGroupPtr> in_sink_inst_retire_grp_ {&unit_port_set_, "in_sink_retire_inst_grp",
                                                                        sparta::SchedulingPhase::Tick, 1};
            sparta::DataOutPort<olympia::InstPtr>     out_rob_retire_ack_ {&unit_port_set_, "out_rob_retire_ack"};
    };
    using IssueQueueSinkUnitFactory = sparta::ResourceFactory<IssueQueueSinkUnit, IssueQueueSinkUnit::SinkUnitParameters>;
}
