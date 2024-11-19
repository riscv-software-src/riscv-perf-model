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
#include "sparta/statistics/WeightedContextCounter.hpp"
#include "sparta/simulation/ResourceFactory.hpp"

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
            DispatchParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

            PARAMETER(uint32_t, num_to_dispatch, 3, "Number of instructions to dispatch")
            PARAMETER(uint32_t, dispatch_queue_depth, 10, "Depth of the dispatch buffer")
            PARAMETER(std::vector<double>, context_weights, std::vector<double>(1, 1),
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
        Dispatch(sparta::TreeNode* node, const DispatchParameterSet* p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static const char name[];

        //! \brief Called by the Dispatchers to indicate when they are ready
        void scheduleDispatchSession();

      private:
        std::unordered_map<InstArchInfo::TargetPipe, std::vector<Dispatcher*>>
            pipe_to_dispatcher_map_;
        InstQueue dispatch_queue_;

        // Ports
        sparta::DataInPort<InstGroupPtr> in_dispatch_queue_write_{&unit_port_set_,
                                                                  "in_dispatch_queue_write", 1};
        sparta::DataOutPort<uint32_t> out_dispatch_queue_credits_{&unit_port_set_,
                                                                  "out_dispatch_queue_credits"};

        // Dynamic ports for execution resources (based on core
        // extension's execution_topology)
        std::vector<std::unique_ptr<sparta::DataInPort<uint32_t>>> in_credit_ports_;
        std::vector<std::unique_ptr<sparta::DataOutPort<InstQueue::value_type>>> out_inst_ports_;

        sparta::DataInPort<uint32_t> in_lsu_credits_{&unit_port_set_, "in_lsu_credits",
                                                     sparta::SchedulingPhase::Tick, 0};
        sparta::DataOutPort<InstQueue::value_type> out_lsu_write_{&unit_port_set_, "out_lsu_write",
                                                                  false};
        sparta::DataInPort<uint32_t> in_reorder_credits_{
            &unit_port_set_, "in_reorder_buffer_credits", sparta::SchedulingPhase::Tick, 0};
        sparta::DataOutPort<InstGroupPtr> out_reorder_write_{&unit_port_set_,
                                                             "out_reorder_buffer_write"};

        std::array<std::vector<std::shared_ptr<Dispatcher>>, InstArchInfo::N_TARGET_PIPES>
            dispatchers_;
        InstArchInfo::TargetPipe blocking_dispatcher_ = InstArchInfo::TargetPipe::UNKNOWN;

        // For flush
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_{
            &unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};

        // Tick events
        sparta::SingleCycleUniqueEvent<> ev_dispatch_insts_{
            &unit_event_set_, "dispatch_event",
            CREATE_SPARTA_HANDLER(Dispatch, dispatchInstructions_)};

        const uint32_t num_to_dispatch_;
        uint32_t credits_rob_ = 0;
        uint32_t dispatch_queue_depth_;

        // Send rename initial credits
        void sendInitialCredits_();

        // Tick callbacks assigned to Ports -- zero cycle
        void robCredits_(const uint32_t &);

        // Dispatch instructions
        void dispatchQueueAppended_(const InstGroupPtr &);
        void dispatchInstructions_();

        // Flush notifications
        void handleFlush_(const FlushManager::FlushingCriteria & criteria);

        ///////////////////////////////////////////////////////////////////////
        // Stall counters
        enum StallReason : uint16_t
        {
            BR_BUSY =        InstArchInfo::TargetPipe::BR,
            CMOV_BUSY =      InstArchInfo::TargetPipe::CMOV,
            DIV_BUSY =       InstArchInfo::TargetPipe::DIV,
            FADDSUB_BUSY =   InstArchInfo::TargetPipe::FADDSUB,
            FLOAT_BUSY =     InstArchInfo::TargetPipe::FLOAT,
            FMAC_BUSY =      InstArchInfo::TargetPipe::FMAC,
            I2F_BUSY =       InstArchInfo::TargetPipe::I2F,
            F2I_BUSY =       InstArchInfo::TargetPipe::F2I,
            INT_BUSY =       InstArchInfo::TargetPipe::INT,
            LSU_BUSY =       InstArchInfo::TargetPipe::LSU,
            MUL_BUSY =       InstArchInfo::TargetPipe::MUL,
            VINT_BUSY =      InstArchInfo::TargetPipe::VINT,
            VFIXED_BUSY =    InstArchInfo::TargetPipe::VFIXED,
            VFLOAT_BUSY =    InstArchInfo::TargetPipe::VFLOAT,
            VFMUL_BUSY =     InstArchInfo::TargetPipe::VFMUL,
            VFDIV_BUSY =     InstArchInfo::TargetPipe::VFDIV,
            VMASK_BUSY =     InstArchInfo::TargetPipe::VMASK,
            VMUL_BUSY =      InstArchInfo::TargetPipe::VMUL,
            VDIV_BUSY =      InstArchInfo::TargetPipe::VDIV,
            VSET_BUSY =      InstArchInfo::TargetPipe::VSET,
            NO_ROB_CREDITS = InstArchInfo::TargetPipe::SYS, // No credits from the ROB
            NOT_STALLED, // Made forward progress (dispatched all instructions or no instructions)
            N_STALL_REASONS
        };

        StallReason current_stall_ = NOT_STALLED;
        friend std::ostream & operator<<(std::ostream &, const Dispatch::StallReason &);

        // Counters -- this is only supported in C++11 -- uses
        // Counter's move semantics
        std::array<sparta::CycleCounter, N_STALL_REASONS> stall_counters_{
            {
                sparta::CycleCounter(getStatisticSet(), "stall_br_busy", "BR busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_cmov_busy", "CMOV busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_div_busy", "DIV busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_faddsub_busy", "FADDSUB busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_float_busy", "FLOAT busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_fmac_busy", "FMAC busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_i2f_busy", "I2F busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_f2i_busy", "F2I busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_int_busy", "INT busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_lsu_busy", "LSU busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_mul_busy", "MUL busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_vint_busy", "VINT busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_vfixed_busy", "VFIXED busy",
                                  sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_vfloat_busy", "VFLOAT busy",
                                  sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_vfmul_busy", "VFMUL busy",
                                  sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_vfdiv_busy", "VFDIV busy",
                                  sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_vmask_busy", "VMASK busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_vmul_busy", "VMUL busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_vdiv_busy", "VDIV busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_vset_busy", "VSET busy",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_rob_full", "No credits from ROB",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_not_stalled",
                                     "Dispatch not stalled, all instructions dispatched",
                                     sparta::Counter::COUNT_NORMAL, getClock())
            }
        };

        std::array<sparta::Counter, InstArchInfo::N_TARGET_PIPES> unit_distribution_{
            {
                sparta::Counter(getStatisticSet(), "count_br_insts", "Total BR insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_cmov_insts", "Total CMOV insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_div_insts", "Total DIV insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_faddsub_insts", "Total FADDSUB insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_float_insts", "Total FLOAT insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_fmac_insts", "Total FMAC insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_i2f_insts", "Total I2F insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_f2i_insts", "Total F2I insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_int_insts", "Total INT insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_lsu_insts", "Total LSU insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_mul_insts", "Total MUL insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_vint_insts", "Total VINT insts",
                             sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_vfixed_insts", "Total VFIXED insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_vfloat_insts", "Total VFLOAT insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_vfmul_insts", "Total VFMUL insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_vfdiv_insts", "Total VFDIV insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_vmask_insts", "Total VMASK insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_vmul_insts", "Total VMUL insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_vdiv_insts", "Total VDIV insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_vset_insts", "Total VSET insts",
                                sparta::Counter::COUNT_NORMAL),
                sparta::Counter(getStatisticSet(), "count_sys_insts", "Total SYS insts",
                                sparta::Counter::COUNT_NORMAL)
            }
        };

        // As an example, this is a context counter that does the same
        // thing as the unit_distribution counter, albeit a little
        // ambiguous as to the relation of the context and the unit.
        sparta::ContextCounter<sparta::Counter> unit_distribution_context_{
            getStatisticSet(),
            "count_insts_per_unit",
            "Unit distributions",
            InstArchInfo::N_TARGET_PIPES,
            "dispatch_inst_count",
            sparta::Counter::COUNT_NORMAL,
            sparta::InstrumentationNode::VIS_NORMAL};

        // As another example, this is a weighted context counter. It does
        // the same thing as a regular sparta::ContextCounter with the addition
        // of being able to specify weights to the various contexts. Calling
        // the "calculatedWeightedAverage()" method will compute the weighted
        // average of the internal context counter values.
        sparta::WeightedContextCounter<sparta::Counter> weighted_unit_distribution_context_{
            getStatisticSet(),
            "weighted_count_insts_per_unit",
            "Weighted unit distributions",
            InstArchInfo::N_TARGET_PIPES,
            sparta::CounterBase::COUNT_NORMAL,
            sparta::InstrumentationNode::VIS_NORMAL};

        // ContextCounter with only one context. These are handled differently
        // than other ContextCounters; they are not automatically expanded to
        // include per-context information in reports, since that is redundant
        // information.
        sparta::ContextCounter<sparta::Counter> int_context_{
            getStatisticSet(),
            "context_count_int_insts",
            "INT instruction count",
            1,
            "dispatch_int_inst_count",
            sparta::CounterBase::COUNT_NORMAL,
            sparta::InstrumentationNode::VIS_NORMAL};
        sparta::StatisticDef total_insts_{
            getStatisticSet(), "count_total_insts_dispatched",
            "Total number of instructions dispatched", getStatisticSet(),
            "count_cmov_insts + count_div_insts + count_faddsub_insts + count_float_insts + "
            "count_fmac_insts + count_i2f_insts + count_f2i_insts + count_int_insts + "
            "count_lsu_insts + count_mul_insts + count_br_insts"};

        friend class DispatchTester;
    };

    using DispatchFactory =
        sparta::ResourceFactory<olympia::Dispatch, olympia::Dispatch::DispatchParameterSet>;

    inline std::ostream & operator<<(std::ostream & os, const Dispatch::StallReason & stall)
    {
        if (stall == Dispatch::StallReason::NOT_STALLED)
        {
            os << "NOT_STALLED";
        }
        else if (stall == Dispatch::StallReason::NO_ROB_CREDITS)
        {
            os << "NO_ROB_CREDITS";
        }
        else if (stall != Dispatch::StallReason::N_STALL_REASONS)
        {
            os << InstArchInfo::execution_pipe_string_map.at((InstArchInfo::TargetPipe)stall) << "_BUSY";
        }
        else
        {
            sparta_assert(false, "How'd we get here?");
        }

        return os;

    }

    class DispatchTester;
} // namespace olympia
