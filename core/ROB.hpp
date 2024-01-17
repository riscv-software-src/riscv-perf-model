// <ROB.h> -*- C++ -*-


#pragma once
#include <string>

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/log/MessageSource.hpp"

#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/StatisticInstance.hpp"

#include "CoreTypes.hpp"
#include "InstGroup.hpp"
#include "FlushManager.hpp"

namespace olympia
{

    /**
     * @file   ROB.h
     * @brief
     *
     * The Reorder buffer will
     * 1. retire and writeback completed instructions
     */
    class ROB : public sparta::Unit
    {
    public:
        //! \brief Parameters for ROB model
        class ROBParameterSet : public sparta::ParameterSet
        {
        public:
            ROBParameterSet(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            { }

            PARAMETER(uint32_t, num_to_retire,       4, "Number of instructions to retire")
            PARAMETER(uint32_t, retire_queue_depth, 30, "Depth of the retire queue")
            PARAMETER(uint32_t, num_insts_to_retire, 0,
                      "Number of instructions to retire after which simulation will be "
                      "terminated. 0 means simulation will run until end of testcase")
            PARAMETER(uint64_t, retire_heartbeat, 1000000, "Heartbeat printout threshold")
            PARAMETER(sparta::Clock::Cycle, retire_timeout_interval, 10000,
                      "Retire timeout error threshold (in cycles). Amount of time elapsed when nothing was retired")
        };

        /**
         * @brief Constructor for ROB
         *
         * @param node The node that represents (has a pointer to) the ROB
         * @param p The ROB's parameter set
         *
         * In the constructor for the unit, it is expected that the user
         * register the TypedPorts that this unit will need to perform
         * work.
         */
        ROB(sparta::TreeNode * node,
            const ROBParameterSet * p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static const char name[];

        /// Destroy!
        ~ROB();

    private:

        // Stats and counters
        sparta::StatisticDef       stat_ipc_;            // A simple expression to calculate IPC
        sparta::Counter            num_retired_;         // Running counter of number instructions retired
        sparta::Counter            num_flushes_;         // Number of flushes
        sparta::StatisticInstance  overall_ipc_si_;      // An overall IPC statistic instance starting at time == 0
        sparta::StatisticInstance  period_ipc_si_;       // An IPC counter for the period between retirement heartbeats

        // Parameter constants
        const sparta::Clock::Cycle retire_timeout_interval_;
        const uint32_t num_to_retire_;
        const uint32_t num_insts_to_retire_; // parameter from ilimit
        const uint64_t retire_heartbeat_;    // Retire heartbeat interval

        InstQueue      reorder_buffer_;

        // Bool that indicates if the ROB stopped simulation.  If
        // false and there are still instructions in the reorder
        // buffer, the machine probably has a lock up
        bool rob_stopped_simulation_{false};

        // Track a program ID to ensure the trace stream matches
        // at retirement.
        uint64_t expected_program_id_ = 1;

        // Ports used by the ROB
        sparta::DataInPort<InstGroupPtr> in_reorder_buffer_write_{&unit_port_set_, "in_reorder_buffer_write", 1};
        sparta::DataOutPort<uint32_t> out_reorder_buffer_credits_{&unit_port_set_, "out_reorder_buffer_credits"};
        sparta::DataInPort<bool>      in_oldest_completed_       {&unit_port_set_, "in_reorder_oldest_completed"};
        sparta::DataOutPort<FlushManager::FlushingCriteria> out_retire_flush_ {&unit_port_set_, "out_retire_flush"};
        // UPDATE:
        sparta::DataOutPort<InstPtr> out_rob_retire_ack_         {&unit_port_set_, "out_rob_retire_ack"};
        sparta::DataOutPort<InstPtr> out_rob_retire_ack_rename_  {&unit_port_set_, "out_rob_retire_ack_rename"};

        // For flush
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_
             {&unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};

        // Events used by the ROB
        sparta::UniqueEvent<> ev_retire_ {&unit_event_set_, "retire_insts",
                CREATE_SPARTA_HANDLER(ROB, retireEvent_)};

        // A nice checker to make sure forward progress is being made
        // Note that in the ROB constructor, this event is set as non-continuing
        sparta::Clock::Cycle last_retirement_ = 0; // Last retirement cycle for checking stalled retire
        sparta::Event<> ev_ensure_forward_progress_{&unit_event_set_, "forward_progress_check",
                CREATE_SPARTA_HANDLER(ROB, checkForwardProgress_)};

        std::unique_ptr<sparta::NotificationSource<bool>> rob_stopped_notif_source_;

        void sendInitialCredits_();
        void retireEvent_();
        void robAppended_(const InstGroup &);
        void retireInstructions_();
        void checkForwardProgress_();
        void handleFlush_(const FlushManager::FlushingCriteria & criteria);
        void dumpDebugContent_(std::ostream& output) const override final;
        void onStartingTeardown_() override final;

    };
}
