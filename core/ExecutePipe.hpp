// <ExecutePipe.hpp> -*- C++ -*-

/**
 * \file  ExecutePipe.hpp
 * \brief Definition of an Execution Pipe
 *
 *
 */

#pragma once

#include "sparta/collection/Collectable.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/resources/Scoreboard.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Unit.hpp"

#include "CoreTypes.hpp"
#include "FlushManager.hpp"
#include "Inst.hpp"

namespace olympia
{
    /**
     * \class ExecutePipe
     * \brief Defines the stages for an execution pipe
     */
    class ExecutePipe : public sparta::Unit
    {

      public:
        //! \brief Parameters for ExecutePipe model
        class ExecutePipeParameterSet : public sparta::ParameterSet
        {
          public:
            ExecutePipeParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}
            PARAMETER(bool, ignore_inst_execute_time, false,
                      "Ignore the instruction's execute time, "
                      "use execute_time param instead")
            PARAMETER(uint32_t, execute_time, 1, "Time for execution")
            HIDDEN_PARAMETER(bool, enable_random_misprediction, false,
                             "test mode to inject random branch mispredictions")
        };

        /**
         * @brief Constructor for ExecutePipe
         *
         * @param node The node that represents (has a pointer to) the ExecutePipe
         * @param p The ExecutePipe's parameter set
         */
        ExecutePipe(sparta::TreeNode* node, const ExecutePipeParameterSet* p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static const char name[];

        bool canAccept() { return !unit_busy_; }

        // Write result to registers
        void insertInst(const InstPtr &);

      private:
        // Ports and the set -- remove the ", 1" to experience a DAG issue!
        sparta::DataInPort<InstQueue::value_type> in_execute_inst_{&unit_port_set_,
                                                                   "in_execute_write", 1};
        sparta::DataOutPort<uint32_t> out_scheduler_credits_{&unit_port_set_,
                                                             "out_scheduler_credits"};
        sparta::DataOutPort<uint32_t> out_execute_pipe_{&unit_port_set_, "out_execute_pipe"};
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_{
            &unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};
        sparta::DataOutPort<FlushManager::FlushingCriteria> out_execute_flush_{&unit_port_set_,
                                                                               "out_execute_flush"};

        // Scoreboards
        using ScoreboardViews =
            std::array<std::unique_ptr<sparta::ScoreboardView>, core_types::N_REGFILES>;
        ScoreboardViews scoreboard_views_;

        // Busy signal for the attached alu
        bool unit_busy_ = false;
        // Execution unit's execution time
        const bool ignore_inst_execute_time_ = false;
        const uint32_t execute_time_;
        const bool enable_random_misprediction_;
        const core_types::RegFile reg_file_;

        // Events used to issue, execute and complete the instruction
        sparta::UniqueEvent<> issue_inst_{
            &unit_event_set_, getName() + "_insert_inst",
            CREATE_SPARTA_HANDLER_WITH_DATA(ExecutePipe, insertInst, InstPtr)};
        sparta::PayloadEvent<InstPtr> execute_inst_{
            &unit_event_set_, getName() + "_execute_inst",
            CREATE_SPARTA_HANDLER_WITH_DATA(ExecutePipe, executeInst_, InstPtr)};
        sparta::PayloadEvent<InstPtr> complete_inst_{
            &unit_event_set_, getName() + "_complete_inst",
            CREATE_SPARTA_HANDLER_WITH_DATA(ExecutePipe, completeInst_, InstPtr)};

        // A pipeline collector
        sparta::collection::Collectable<InstPtr> collected_inst_;

        // Counter
        sparta::Counter total_insts_executed_{getStatisticSet(), "total_insts_executed",
                                              "Total instructions executed",
                                              sparta::Counter::COUNT_NORMAL};

        void setupExecutePipe_();
        void executeInst_(const InstPtr &);

        // Callback from Scoreboard to inform Operand Readiness
        // void handleOperandIssueCheck_(const InstPtr &);
        // Used to complete the inst in the FPU
        void completeInst_(const InstPtr &);

        // Used to flush the ALU
        void flushInst_(const FlushManager::FlushingCriteria &);

        // Friend class used in rename testing
        friend class ExecutePipeTester;
    };
    class ExecutePipeTester;
    using ExecutePipeFactory =
        sparta::ResourceFactory<olympia::ExecutePipe,
                                olympia::ExecutePipe::ExecutePipeParameterSet>;
} // namespace olympia
