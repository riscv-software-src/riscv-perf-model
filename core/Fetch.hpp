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

#include "cache/TreePLRUReplacement.hpp"
#include "cache/BasicCacheItem.hpp"
#include "cache/Cache.hpp"
// #include "cache/ReplacementIF.hpp"


#include "CoreTypes.hpp"
#include "InstGroup.hpp"

namespace olympia
{
    class InstGenerator;


    class BTBEntry : public sparta::cache::BasicCacheItem
    {
    public:
        BTBEntry() :
            valid_(false)
        { };

        BTBEntry(const BTBEntry &rhs) :
            BasicCacheItem(rhs),
            valid_(rhs.valid_),
            target_(rhs.target_)
        { };

        BTBEntry &operator=(const BTBEntry &rhs)
        {
            if (&rhs != this) {
                BasicCacheItem::operator=(rhs);
                valid_ = rhs.valid_;
                target_ = rhs.target_;
            }
            return *this;
        }

        void reset(uint64_t addr, uint64_t target)
        {
            setValid(true);
            setAddr(addr);
            setTarget(target);
        }

        void setValid(bool v) { valid_ = v; }
        bool isValid() const { return valid_; }

        void setTarget(uint64_t t) { target_ = t; }
        uint64_t getTarget() const { return target_; }

    private:
        bool valid_ = false;
        uint64_t target_;

    };

    class BTB : public sparta::cache::Cache<BTBEntry>
    {
    private:
        class BTBAddrDecoder : public sparta::cache::AddrDecoderIF
        {
        public:
            BTBAddrDecoder(uint16_t stride, uint16_t size) : stride_(stride), size_(size) {}
            virtual uint64_t calcTag(uint64_t addr) const { return addr; }
            virtual uint32_t calcIdx(uint64_t addr) const { return (addr >> 6) & 511; }
            virtual uint64_t calcBlockAddr(uint64_t addr) const { return addr; }
            virtual uint64_t calcBlockOffset(uint64_t addr) const { return 0;}
        private:
            const uint16_t stride_;
            const uint16_t size_;

            // If we've got 4096 entries, 64B stride and 8 ways
            // sets = 4096/8 = 512 = 2**9
            // Index = (address >> 6) & 2**9-1
            // or      (address / 64) % 512
            // or      address[15:6]
            // Generically it'll be mask = sets-1, shift = log2(stride)
        };
    public:
        BTB() :
            sparta::cache::Cache<BTBEntry> (4096, 1, 512, BTBEntry(), sparta::cache::TreePLRUReplacement(8), false)
        {
            setAddrDecoder(new BTBAddrDecoder(4096, 64));
        }
    };

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
        sparta::DataInPort<uint64_t> in_fetch_flush_redirect_
            {&unit_port_set_, "in_fetch_flush_redirect", sparta::SchedulingPhase::Flush, 1};

        // Retired Instruction
        sparta::DataInPort<InstPtr> in_rob_retire_ack_ {&unit_port_set_, "in_rob_retire_ack", 1};

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

        // BTB
        std::unique_ptr<BTB> btb_;

        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks

        // Fire Fetch up
        void initialize_();

        // Receive the number of free credits from decode
        void receiveFetchQueueCredits_(const uint32_t &);

        // Read data from a trace
        void fetchInstruction_();

        // Receive flush from retire
        void flushFetch_(const uint64_t & new_addr);

        void getAckFromROB_(const InstPtr &);

        bool predictInstruction_(InstPtr inst);

        // Are we fetching a speculative path?
        bool speculative_path_ = false;
    };

}
