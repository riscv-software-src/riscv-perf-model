// <ExecutePipe.hpp> -*- C++ -*-


/**
 * \file  ExecutePipe.hpp
 * \brief Definition of an Execution Pipe
 *
 *
 */

#pragma once

#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/collection/Collectable.hpp"
#include "sparta/resources/Scoreboard.hpp"

#include "Inst.hpp"
#include "CoreTypes.hpp"
#include "FlushManager.hpp"

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
            ExecutePipeParameterSet(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            { }
            PARAMETER(bool, ignore_inst_execute_time, false,
                      "Ignore the instruction's execute time, "
                      "use execute_time param instead")
            PARAMETER(uint32_t, execute_time, 1, "Time for execution")
            PARAMETER(uint32_t, scheduler_size, 8, "Scheduler queue size")
            PARAMETER(bool, in_order_issue, true, "Force in order issue")
        };

        /**
         * @brief Constructor for ExecutePipe
         *
         * @param node The node that represents (has a pointer to) the ExecutePipe
         * @param p The ExecutePipe's parameter set
         */
        ExecutePipe(sparta::TreeNode * node,
                    const ExecutePipeParameterSet * p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static const char name[];

    private:
        // Ports and the set -- remove the ", 1" to experience a DAG issue!
        sparta::DataInPort<InstQueue::value_type> in_execute_inst_ {
            &unit_port_set_, "in_execute_write", 1};
        sparta::DataOutPort<uint32_t> out_scheduler_credits_{&unit_port_set_, "out_scheduler_credits"};
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_
            {&unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};
        sparta::DataOutPort<FlushManager::FlushingCriteria> out_execute_flush_
            {&unit_port_set_, "out_execute_flush"};

        // Ready queue
        typedef std::list<InstPtr> ReadyQueue;
        ReadyQueue  ready_queue_;
        ReadyQueue  issue_queue_;

        // Scoreboards
        using ScoreboardViews = std::array<std::unique_ptr<sparta::ScoreboardView>, core_types::N_REGFILES>;
        ScoreboardViews scoreboard_views_;

        // Busy signal for the attached alu
        bool unit_busy_ = false;
        // Execution unit's execution time
        const bool     ignore_inst_execute_time_ = false;
        const uint32_t execute_time_;
        const uint32_t scheduler_size_;
        const bool in_order_issue_;
        const core_types::RegFile reg_file_;
        sparta::collection::IterableCollector<std::list<InstPtr>>
        ready_queue_collector_ {getContainer(), "scheduler_queue",
                &ready_queue_, scheduler_size_};

        // Events used to issue and complete the instruction
        sparta::UniqueEvent<> issue_inst_{&unit_event_set_, getName() + "_issue_inst",
                CREATE_SPARTA_HANDLER(ExecutePipe, issueInst_)};
        sparta::PayloadEvent<InstPtr> complete_inst_{
            &unit_event_set_, getName() + "_complete_inst",
            CREATE_SPARTA_HANDLER_WITH_DATA(ExecutePipe, completeInst_, InstPtr)};

        // A pipeline collector
        sparta::collection::Collectable<InstPtr> collected_inst_;

        // Counter
        sparta::Counter total_insts_issued_{
            getStatisticSet(), "total_insts_issued",
            "Total instructions issued", sparta::Counter::COUNT_NORMAL
        };
        sparta::Counter total_insts_executed_{
            getStatisticSet(), "total_insts_executed",
            "Total instructions executed", sparta::Counter::COUNT_NORMAL
        };

        void setupExecutePipe_();

        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks
        void issueInst_();
        void getInstsFromDispatch_(const InstPtr&);

        // Callback from Scoreboard to inform Operand Readiness
        void handleOperandIssueCheck_(const InstPtr &);

        // Used to complete the inst in the FPU
        void completeInst_(const InstPtr&);

        // Used to flush the ALU
        void flushInst_(const FlushManager::FlushingCriteria &);

        ////////////////////////////////////////////////////////////////////////////////
        // Regular Function/Subroutine Call

        // Append new instruction into issue queue
        void appendIssueQueue_(const InstPtr &);

        // Pop completed instruction out of issue queue
        void popIssueQueue_(const InstPtr &);

        // Friend class used in rename testing
        friend class ExecutePipeTester;
    };
    class ExecutePipeTester;
    using ExecutePipeFactory = sparta::ResourceFactory<olympia::ExecutePipe,
                                                       olympia::ExecutePipe::ExecutePipeParameterSet>;
} // namespace olympia
