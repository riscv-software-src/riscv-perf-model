#pragma once

#include "core/InstGenerator.hpp"

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/LogUtils.hpp"

#include <string>

//FIXME remove before push 
#include <iostream>
using namespace std;

namespace core_test
{
// ----------------------------------------------------------------------
//  "Sink" unit, just sinks instructions sent to it.  Sends credits
//  back as directed by params/execution mode
// ----------------------------------------------------------------------
class Sink : public sparta::Unit
{
  public:
    static constexpr char name[] = "sink";

    class SinkParameters : public sparta::ParameterSet
    {
      public:
        explicit SinkParameters(sparta::TreeNode* n) :
            sparta::ParameterSet(n)
        {
        }

        PARAMETER(uint32_t, sink_queue_size, 10,
                  "Sink queue size for testing")

        PARAMETER(std::string, purpose, "grp",
                  "Purpose of this Sink: grp, single")
    };

    Sink(sparta::TreeNode* n, const SinkParameters* params) :
        sparta::Unit(n),
        credits_(params->sink_queue_size),
        credits_to_send_back_(credits_)
    {
        if (params->purpose == "grp")
        {
            i_instgrp_write_.registerConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(
                    Sink, sinkInst_<olympia::InstGroupPtr>,
                    olympia::InstGroupPtr));
        }
        else if (params->purpose == "single")
        {
            i_instgrp_write_.registerConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(
                    Sink, sinkInst_<olympia::InstPtr>,
                    olympia::InstPtr));
        }
        else
        {
          throw std::invalid_argument(
                "Usage: purpose must be one of 'grp' or 'single'");
        }
        sparta::StartupEvent(
            n, CREATE_SPARTA_HANDLER(Sink, sendCredits_));
    }

  private:
    template <class InstType>
    void sinkInst_(const InstType & inst_or_insts)
    {
        if(credits_ > 0) --credits_;
        if constexpr (std::is_same_v<InstType, olympia::InstGroupPtr>)
        {
            for (auto ptr : *inst_or_insts)
            {
                ILOG("Instruction: '" << ptr << "' sinked");
            }
        }
        else
        {
            ILOG("Instruction: '" << inst_or_insts << "' sinked");
        }
        ++credits_to_send_back_;
        ev_return_credits_.schedule(1);
    }

    void sendCredits_()
    {
        o_restore_credits_.send(credits_to_send_back_);
        credits_ += credits_to_send_back_;
        credits_to_send_back_ = 0;
    }

    sparta::DataOutPort<uint32_t> o_restore_credits_{&unit_port_set_,
                                                    "o_restore_credits"};

    sparta::DataInPort<olympia::InstGroupPtr> i_instgrp_write_{
        &unit_port_set_, "i_instgrp_write", sparta::SchedulingPhase::Tick, 1};

    uint32_t credits_ = 0;
    sparta::UniqueEvent<> ev_return_credits_{
        &unit_event_set_, "return_credits",
        CREATE_SPARTA_HANDLER(Sink, sendCredits_)};
    uint32_t credits_to_send_back_ = 0;
};

using SinkFactory =
    sparta::ResourceFactory<Sink, Sink::SinkParameters>;
} // namespace core_test
