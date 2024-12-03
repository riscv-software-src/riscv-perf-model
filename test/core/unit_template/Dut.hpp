#pragma once

#include "FlushManager.hpp"
#include "InstGroup.hpp"

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

#include <string>

namespace olympia
{
    class Dut : public sparta::Unit
    {
      public:
        //! \brief Parameters for Dut model
        class DutParameterSet : public sparta::ParameterSet
        {
          public:
            DutParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

            PARAMETER(uint32_t, num_to_process, 4, "Number of instructions to process")
            PARAMETER(uint32_t, input_queue_size, 10, "Size of the input queue")
        };

        Dut(sparta::TreeNode* node, const DutParameterSet* p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static constexpr char name[] = "dut";

      private:
        // written by src unit
        InstQueue input_queue_;

        // Port listening to the in-queue appends - Note the 1 cycle
        // src to dut instrgrp
        sparta::DataInPort<InstGroupPtr> i_instgrp_write_{&unit_port_set_, "i_instgrp_write", 1};

        // dut to src restore credits
        sparta::DataOutPort<uint32_t> o_restore_credits_{&unit_port_set_, "o_restore_credits"};

        // dut to sink instgrp
        sparta::DataOutPort<InstGroupPtr> o_instgrp_write_{&unit_port_set_, "o_instgrp_write"};

        // sink to dut restore credits
        sparta::DataInPort<uint32_t> i_credits_{&unit_port_set_, "i_credits",
                                                sparta::SchedulingPhase::Tick, 0};

        // For flush
        sparta::DataInPort<FlushManager::FlushingCriteria> i_dut_flush_{
            &unit_port_set_, "i_dut_flush", sparta::SchedulingPhase::Flush, 1};

        // The process instruction event
        sparta::UniqueEvent<> ev_process_insts_event_{&unit_event_set_, "process_insts_event",
                                                      CREATE_SPARTA_HANDLER(Dut, processInsts_)};

        // Dut callbacks
        void sendInitialCredits_();
        void inputQueueAppended_(const InstGroupPtr &);
        void receiveInpQueueCredits_(const uint32_t &);
        void processInsts_();
        void handleFlush_(const FlushManager::FlushingCriteria & criteria);

        uint32_t inp_queue_credits_ = 0;
        const uint32_t num_to_process_;
    };
} // namespace olympia
