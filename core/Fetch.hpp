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
#include "MemoryAccessInfo.hpp"

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
            PARAMETER(uint32_t, block_width,          16, "Block width of memory read requests, in bytes")
            PARAMETER(uint32_t, target_queue_size,    16, "Size of the fetch target queue")
            PARAMETER(uint32_t, fetch_buffer_size,     8, "Size of fetch buffer")
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

        // Instruction Cache Request
        sparta::DataOutPort<MemoryAccessInfoPtr> out_fetch_icache_req_
            {&unit_port_set_, "out_fetch_icache_req"};

        // Instruction Cache Response
        sparta::DataInPort<MemoryAccessInfoPtr> in_icache_fetch_resp_
            {&unit_port_set_, "in_icache_fetch_resp", sparta::SchedulingPhase::Tick, 1};

        // Instruction Cache Credit
        sparta::DataInPort<uint32_t> in_icache_fetch_credits_
            {&unit_port_set_, "in_icache_fetch_credits", sparta::SchedulingPhase::Tick, 0};

        ////////////////////////////////////////////////////////////////////////////////
        // Instruction fetch

        const bool disabled_bpred_ = true;

        // Unit's clock
        const sparta::Clock * my_clk_ = nullptr;
        // Number of instructions to fetch
        const uint32_t num_insts_to_fetch_;

        // For traces with system instructions, skip them
        const bool skip_nonuser_mode_;

        // Number of credits from decode that fetch has
        uint32_t credits_inst_queue_ = 0;

        // Number of credits available in the ICache
        uint32_t credits_icache_ = 0;

        // Amount to left shift an Instructions PC to get the ICache block number
        const uint32_t icache_block_shift_;

        // Buffers up instructions read from the tracefile
        std::deque<InstPtr> ibuf_;

        // Size of trace buffer (must be sized >= L1ICache bandwidth / 2B)
        const uint32_t ibuf_capacity_;

        sparta::Queue<InstGroupPtr> target_buffer_;

        // Fetch buffer: Holds a queue of instructions that are either
        // waiting for an ICache hit response, or they're ready to be
        // send to decode
        sparta::Queue<InstGroupPtr> fetch_buffer_;

        // allocator for ICache transactions
        MemoryAccessInfoAllocator & memory_access_allocator_;

        // ROB terminated simulation
        bool rob_stopped_simulation_ {false};

        // Instruction generation
        std::unique_ptr<InstGenerator> inst_generator_;

        std::unique_ptr<sparta::SingleCycleUniqueEvent<>> ev_predict_insts_;

        // Fetch instruction event, the callback is set to request
        // instructions from the instruction cache and place them in the
        // fetch buffer.
        std::unique_ptr<sparta::SingleCycleUniqueEvent<>> ev_fetch_insts_;

        // Send instructions event, the callback is set to read instructions
        // from the fetch buffer and send them to the decode unit
        std::unique_ptr<sparta::SingleCycleUniqueEvent<>> ev_send_insts_;

        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks

        // Fire Fetch up
        void initialize_();

        // Receive the number of free credits from decode
        void receiveFetchQueueCredits_(const uint32_t &);

        void doBranchPrediction_();

        // Read data from a trace
        void fetchInstruction_();

        // Read instructions from the fetch buffer and send them to decode
        void sendInstructions_();

        // Receive flush from FlushManager
        void flushFetch_(const FlushManager::FlushingCriteria &);

        // Receieve the number of free credits from the instruction cache
        void receiveCacheCredit_(const uint32_t &);

        // Receive read data from the instruction cache
        void receiveCacheResponse_(const MemoryAccessInfoPtr &);

        // Debug callbacks, used to log fetch buffer contents
        void onROBTerminate_(const bool&);
        void onStartingTeardown_() override;
        void dumpDebugContent_(std::ostream&) const override final;

        // Are we fetching a speculative path?
        bool speculative_path_ = false;
    };

}
