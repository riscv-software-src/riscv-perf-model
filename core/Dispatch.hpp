// <Dispatch.h> -*- C++ -*-
#pragma once

#include <string>
#include <array>
#include <cinttypes>

#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/ContextCounter.hpp"
#include "sparta/simulation/ResourceFactory.hpp"

#include "test/ContextCounter/WeightedContextCounter.hpp"

#include "Dispatcher.hpp"
#include "CoreTypes.hpp"
#include "InstGroup.hpp"
#include "FlushManager.hpp"

namespace olympia
{

    /**
     * @file   Dispatch.h
     * @brief  Class definition for the Dispatch block of the CoreExample
     *
     * Dispatch will
     * 1. Create the dispatch uop queue
     * 2. The dispatch machine will send to unit for execution
     */
    class Dispatch : public sparta::Unit
    {
    public:
        //! \brief Parameters for Dispatch model
        class DispatchParameterSet : public sparta::ParameterSet
        {
        public:
            DispatchParameterSet(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            { }

            PARAMETER(uint32_t, num_to_dispatch,       3, "Number of instructions to dispatch")
            PARAMETER(uint32_t, dispatch_queue_depth, 10, "Depth of the dispatch buffer")
            PARAMETER(std::vector<double>, context_weights, std::vector<double>(1,1),
                      "Relative weight of each context")
        };

        /**
         * @brief Constructor for Dispatch
         *
         * @param name The name of this unit
         * @param container TreeNode which owns this resource.
         *
         * In the constructor for the unit, it is expected that the user
         * register the TypedPorts that this unit will need to perform
         * work.
         */
        Dispatch(sparta::TreeNode * node, const DispatchParameterSet * p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static const char name[];

        //! \brief Called by the Dispatchers to indicate when they are ready
        void scheduleDispatchSession();

    private:
        InstQueue dispatch_queue_;

        // Ports
        sparta::DataInPort<InstGroupPtr>           in_dispatch_queue_write_   {&unit_port_set_, "in_dispatch_queue_write", 1};
        sparta::DataOutPort<uint32_t>              out_dispatch_queue_credits_{&unit_port_set_, "out_dispatch_queue_credits"};

        // Dynamic ports for execution resources (based on core
        // extension's execution_topology)
        std::vector<std::unique_ptr<sparta::DataInPort<uint32_t>>>               in_credit_ports_;
        std::vector<std::unique_ptr<sparta::DataOutPort<InstQueue::value_type>>> out_inst_ports_;

        sparta::DataInPort<uint32_t>               in_lsu_credits_    {&unit_port_set_, "in_lsu_credits",  sparta::SchedulingPhase::Tick, 0};
        sparta::DataOutPort<InstQueue::value_type> out_lsu_write_     {&unit_port_set_, "out_lsu_write", false};
        sparta::DataInPort<uint32_t>               in_reorder_credits_{&unit_port_set_, "in_reorder_buffer_credits", sparta::SchedulingPhase::Tick, 0};
        sparta::DataOutPort<InstGroupPtr>          out_reorder_write_ {&unit_port_set_, "out_reorder_buffer_write"};

        std::array<std::vector<std::unique_ptr<Dispatcher>>, InstArchInfo::N_TARGET_UNITS>  dispatchers_;
        InstArchInfo::TargetUnit blocking_dispatcher_ = InstArchInfo::TargetUnit::NONE;

        // For flush
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_
             {&unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};

        // Tick events
        sparta::SingleCycleUniqueEvent<> ev_dispatch_insts_{&unit_event_set_, "dispatch_event",
                                                            CREATE_SPARTA_HANDLER(Dispatch, dispatchInstructions_)};

        const uint32_t num_to_dispatch_;
        uint32_t credits_rob_ = 0;

        // Send rename initial credits
        void sendInitialCredits_();

        // Tick callbacks assigned to Ports -- zero cycle
        void robCredits_(const uint32_t&);

        // Dispatch instructions
        void dispatchQueueAppended_(const InstGroupPtr &);
        void dispatchInstructions_();

        // Flush notifications
        void handleFlush_(const FlushManager::FlushingCriteria & criteria);

        ///////////////////////////////////////////////////////////////////////
        // Stall counters
        enum StallReason {
            ALU_BUSY = InstArchInfo::TargetUnit::ALU, // Could not send any or all instructions -- ALU busy
            FPU_BUSY = InstArchInfo::TargetUnit::FPU, // Could not send any or all instructions -- FPU busy
            BR_BUSY  = InstArchInfo::TargetUnit::BR,  // Could not send any or all instructions -- BR busy
            LSU_BUSY = InstArchInfo::TargetUnit::LSU,
            NO_ROB_CREDITS = InstArchInfo::TargetUnit::ROB,  // No credits from the ROB
            NOT_STALLED,     // Made forward progress (dipatched all instructions or no instructions)
            N_STALL_REASONS
        };

        StallReason current_stall_ = NOT_STALLED;
        friend std::ostream&operator<<(std::ostream &, const Dispatch::StallReason &);

        // Counters -- this is only supported in C++11 -- uses
        // Counter's move semantics
        std::array<sparta::CycleCounter, N_STALL_REASONS> stall_counters_{{
                sparta::CycleCounter(getStatisticSet(), "stall_alu_busy",
                                     "ALU busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_fpu_busy",
                                     "FPU busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_br_busy",
                                     "BR busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_lsu_busy",
                                     "LSU busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_no_rob_credits",
                                     "No credits from ROB",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_not_stalled",
                                     "Dispatch not stalled, all instructions dispatched",
                                     sparta::Counter::COUNT_NORMAL, getClock())
            }};

        std::array<sparta::Counter,
                   InstArchInfo::N_TARGET_UNITS>
        unit_distribution_ {{
                sparta::Counter(getStatisticSet(), "count_alu_insts",
                                "Total ALU insts", sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_fpu_insts",
                                "Total FPU insts", sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_br_insts",
                                "Total BR insts", sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_lsu_insts",
                                "Total LSU insts", sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_rob_insts",
                                "Total ROB insts", sparta::Counter::COUNT_NORMAL)
            }};

        // As an example, this is a context counter that does the same
        // thing as the unit_distribution counter, albeit a little
        // ambiguous as to the relation of the context and the unit.
        sparta::ContextCounter<sparta::Counter> unit_distribution_context_
        {getStatisticSet(),
                "count_insts_per_unit",
                "Unit distributions",
                InstArchInfo::N_TARGET_UNITS,
                "dispatch_inst_count",
                sparta::Counter::COUNT_NORMAL,
                sparta::InstrumentationNode::VIS_NORMAL};

        // As another example, this is a weighted context counter. It does
        // the same thing as a regular sparta::ContextCounter with the addition
        // of being able to specify weights to the various contexts. Calling
        // the "calculatedWeightedAverage()" method will compute the weighted
        // average of the internal context counter values.
        sparta::WeightedContextCounter<sparta::Counter> weighted_unit_distribution_context_ {
            getStatisticSet(),
            "weighted_count_insts_per_unit",
            "Weighted unit distributions",
            InstArchInfo::N_TARGET_UNITS,
            sparta::CounterBase::COUNT_NORMAL,
            sparta::InstrumentationNode::VIS_NORMAL
        };

        // ContextCounter with only one context. These are handled differently
        // than other ContextCounters; they are not automatically expanded to
        // include per-context information in reports, since that is redundant
        // information.
        sparta::ContextCounter<sparta::Counter> alu_context_ {
            getStatisticSet(),
            "context_count_alu_insts",
            "ALU instruction count",
            1,
            "dispatch_alu_inst_count",
            sparta::CounterBase::COUNT_NORMAL,
            sparta::InstrumentationNode::VIS_NORMAL
        };

        sparta::StatisticDef total_insts_{
            getStatisticSet(), "count_total_insts_dispatched",
            "Total number of instructions dispatched",
            getStatisticSet(), "count_alu_insts + count_fpu_insts + count_lsu_insts"
        };
    };

    using DispatchFactory = sparta::ResourceFactory<olympia::Dispatch,
                                                    olympia::Dispatch::DispatchParameterSet>;

    inline std::ostream&operator<<(std::ostream &os, const Dispatch::StallReason & stall)
    {
        switch(stall)
        {
            case Dispatch::StallReason::NOT_STALLED:
                os << "NOT_STALLED";
                break;
            case Dispatch::StallReason::NO_ROB_CREDITS:
                os << "NO_ROB_CREDITS";
                break;
            case Dispatch::StallReason::ALU_BUSY:
                os << "ALU_BUSY";
                break;
            case Dispatch::StallReason::FPU_BUSY:
                os << "FPU_BUSY";
                break;
            case Dispatch::StallReason::LSU_BUSY:
                os << "LSU_BUSY";
                break;
            case Dispatch::StallReason::BR_BUSY:
                os << "BR_BUSY";
                break;
            case Dispatch::StallReason::N_STALL_REASONS:
                sparta_assert(false, "How'd we get here?");
        }
        return os;
    }
}
