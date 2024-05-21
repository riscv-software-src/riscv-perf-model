// <Fetch.hpp> -*- C++ -*-

//!
//! \file Fetch.hpp
//! \brief Definition of the CoreModel Fetch unit
//!


#pragma once

#include <string>
#include "sparta/ports/DataPort.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/collection/Collectable.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

#include "CoreTypes.hpp"
#include "InstGroup.hpp"
#include "FlushManager.hpp"

namespace olympia
{
    class InstGenerator;

    /**
     * @file   Fetch.h
     * @brief The Fetch block -- gets new instructions to send down the pipe
     *
     * This fetch unit is pretty simple and does not support
     * redirection.  But, if it did, a port between the ROB and Fetch
     * (or Branch and Fetch -- if we had a Branch unit) would be
     * required to release fetch from holding out on branch
     * resolution.
     */
    class Fetch : public sparta::Unit
    {
    public:
        //! \brief Parameters for Fetch model
        class FetchParameterSet : public sparta::ParameterSet
        {
        public:
            FetchParameterSet(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            {
                auto non_zero_validator = [](uint32_t & val, const sparta::TreeNode*)->bool {
                    if(val > 0) {
                        return true;
                    }
                    return false;
                };
                num_to_fetch.addDependentValidationCallback(non_zero_validator,
                                                            "Num to fetch must be greater than 0");
            }

            PARAMETER(uint32_t, num_to_fetch,          4, "Number of instructions to fetch")
            PARAMETER(bool,     skip_nonuser_mode, false, "For STF traces, skip system instructions if present")
            PARAMETER(uint32_t, fetched_queue_size, 8, "Size of the fetched queue for holding instructions we have to wait on vset for")
        };

        /**
         * @brief Constructor for Fetch
         *
         * @param node The node that represents (has a pointer to) the Fetch
         * @param p The Fetch's parameter set
         */
        Fetch(sparta::TreeNode * name,
              const FetchParameterSet * p);

        ~Fetch();

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static const char * name;

    private:

        ////////////////////////////////////////////////////////////////////////////////
        // Ports

        // Internal DataOutPort to the decode unit's fetch queue
        sparta::DataOutPort<InstGroupPtr> out_fetch_queue_write_ {&unit_port_set_, "out_fetch_queue_write"};

        // Internal DataInPort from decode's fetch queue for credits
        sparta::DataInPort<uint32_t> in_fetch_queue_credits_
            {&unit_port_set_, "in_fetch_queue_credits", sparta::SchedulingPhase::Tick, 0};

        // Incoming flush from Retire w/ redirect
        sparta::DataInPort<FlushManager::FlushingCriteria> in_fetch_flush_redirect_
            {&unit_port_set_, "in_fetch_flush_redirect", sparta::SchedulingPhase::Flush, 1};

        // Incoming vset
        sparta::DataInPort<InstPtr> in_vset_inst_{&unit_port_set_, "in_vset_inst", 1};
        ////////////////////////////////////////////////////////////////////////////////
        // Instruction fetch
        // Number of instructions to fetch
        const uint32_t num_insts_to_fetch_;

        // For traces with system instructions, skip them
        const bool skip_nonuser_mode_;

        // Number of credits from decode that fetch has
        uint32_t credits_inst_queue_ = 0;

        // Unit's clock
        const sparta::Clock * my_clk_ = nullptr;

        // Instruction generation
        std::unique_ptr<InstGenerator> inst_generator_;

        // Fetch instruction event, triggered when there are credits
        // from decode.  The callback set is either to fetch random
        // instructions or a perfect IPC set
        std::unique_ptr<sparta::SingleCycleUniqueEvent<>> fetch_inst_event_;

        // waiting on vset flag
        bool waiting_on_vset_ = false;
        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks

        // Fire Fetch up
        void initialize_();

        // Receive the number of free credits from decode
        void receiveFetchQueueCredits_(const uint32_t &);

        // Read data from a trace
        void fetchInstruction_();

        // Receive flush from FlushManager
        void flushFetch_(const FlushManager::FlushingCriteria &);

        // Recieve vset instruction and set waiting_on_vset_ flag
        void process_vset_(const InstPtr &);

        // Are we fetching a speculative path?
        bool speculative_path_ = false;

        // queue to hold fetched instructions that have to be stalled due to vset
        // this is not to hold all fetched instructions!!!
        // we have to hold because we generate instructions using getNextInst()
        // and if we're waiting on vset, we have to hold the instructions until the vset is processed
        InstQueue fetched_queue_;
    };

}
