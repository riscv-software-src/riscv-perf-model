// <Fetch.cpp> -*- C++ -*-

//!
//! \file Fetch.cpp
//! \brief Implementation of the CoreModel Fetch unit
//!

#include <algorithm>
#include "Fetch.hpp"
#include "InstGenerator.hpp"
#include "MavisUnit.hpp"

#include "sparta/utils/LogUtils.hpp"
#include "sparta/events/StartupEvent.hpp"

namespace olympia
{
    const char * Fetch::name = "fetch";

    Fetch::Fetch(sparta::TreeNode * node,
                 const FetchParameterSet * p) :
        sparta::Unit(node),
        num_insts_to_fetch_(p->num_to_fetch),
        skip_nonuser_mode_(p->skip_nonuser_mode),
        my_clk_(getClock())
    {
        in_fetch_queue_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, receiveFetchQueueCredits_, uint32_t));

        fetch_inst_event_.reset(new sparta::SingleCycleUniqueEvent<>(&unit_event_set_, "fetch_random",
                                                                     CREATE_SPARTA_HANDLER(Fetch, fetchInstruction_)));
        // Schedule a single event to start reading from a trace file
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Fetch, initialize_));

        in_fetch_flush_redirect_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, flushFetch_, uint64_t));

        in_rob_retire_ack_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, getAckFromROB_, InstPtr));

        btb_.reset(new BTB());

    }

    Fetch::~Fetch() {}

    void Fetch::initialize_()
    {
        // Get the CPU Node
        auto cpu_node   = getContainer()->getParent()->getParent();
        auto extension  = sparta::notNull(cpu_node->getExtension("simulation_configuration"));
        auto workload   = extension->getParameters()->getParameter("workload");
        inst_generator_ = InstGenerator::createGenerator(getMavis(getContainer()),
                                                         workload->getValueAsString(),
                                                         skip_nonuser_mode_);

        fetch_inst_event_->schedule(1);
    }


    bool Fetch::predictInstruction_(InstPtr inst)
    {
        if (btb_ == nullptr)
        {
            return false;
        }

        auto btb_entry = btb_->getItem(inst->getPC());
        if (btb_entry == nullptr)
        {
            return false;
        }

        // BTB hit
        // auto prediction = sparta::allocate_sparta_shared_pointer<PredictionInfo>(prediction_info_allocator);
        // prediction->taken = true;
        // prediction->target = btb_entry->getTarget();
        // inst->setPrediction(prediction);
        // return prediction->taken;
        return false;
    }

    void Fetch::getAckFromROB_(const InstPtr & inst)
    {
        ILOG("Committed " << inst);
        if (!inst->isBranch() || btb_ == nullptr)
        {
            return;
        }
        // This is all a bit messy...
        auto btb_entry = btb_->peekItem(inst->getPC());
        auto dec = btb_->getAddrDecoder();
        ILOG("Searching BTB pc: " << inst->getPC() << " index: " << dec->calcIdx(inst->getPC()) << " tag: " << dec->calcTag(inst->getPC()));
        if (nullptr == btb_entry)
        {
            // Insert
            // SimpleCache2 has an allocateWithMRUUpdate, but we'd still need to get the item for replacement first..
            auto set = &btb_->getCacheSet(inst->getPC());
            auto rep = set->getReplacementIF();
            auto entry = &set->getItemForReplacementWithInvalidCheck();
            entry->reset(inst->getPC(), inst->getTargetVAddr());
            rep->touchMRU(entry->getWay());
            ILOG("MISS BTB for " << inst);
            ILOG("INSERTED IN BTB at " << entry->getSetIndex() << " " << entry->getWay() << " tag: " << entry->getTag());
        }
        else {
            // SimpleCache2 has a touchMRU function
            sparta_assert(btb_entry->isValid(), "Hit BTB entry is not valid"); // Guess this could happen after an invalidate..
            ILOG("HIT BTB for " << inst);
            auto rep = btb_->getCacheSetAtIndex(btb_entry->getSetIndex()).getReplacementIF();
            rep->touchMRU(btb_entry->getWay());
        }
    }

    void Fetch::fetchInstruction_()
    {
        const uint32_t upper = std::min(credits_inst_queue_, num_insts_to_fetch_);

        // Nothing to send.  Don't need to schedule this again.
        if(upper == 0) { return; }

        InstGroupPtr insts_to_send = sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);
        for(uint32_t i = 0; i < upper; ++i)
        {
            InstPtr ex_inst = inst_generator_->getNextInst(my_clk_);
            if(SPARTA_EXPECT_FALSE(nullptr == ex_inst))
            {
                break;
            }

            ex_inst->setSpeculative(speculative_path_);
            insts_to_send->emplace_back(ex_inst);

            ILOG("Sending: " << ex_inst << " down the pipe");

            if (predictInstruction_(ex_inst))
            {
                break;
            }

        }

        if(false == insts_to_send->empty())
        {
            out_fetch_queue_write_.send(insts_to_send);

            credits_inst_queue_ -= static_cast<uint32_t> (insts_to_send->size());

            if((credits_inst_queue_ > 0) && (false == inst_generator_->isDone())) {
                fetch_inst_event_->schedule(1);
            }

            if(SPARTA_EXPECT_FALSE(info_logger_)) {
                info_logger_ << "Fetch: send num_inst=" << insts_to_send->size()
                             << " instructions, remaining credit=" << credits_inst_queue_;
            }
        }
        else if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Fetch: no instructions from trace";
        }
    }

    // Called when decode has room
    void Fetch::receiveFetchQueueCredits_(const uint32_t & dat) {
        credits_inst_queue_ += dat;

        ILOG("Fetch: receive num_decode_credits=" << dat
             << ", total decode_credits=" << credits_inst_queue_);

        // Schedule a fetch event this cycle
        fetch_inst_event_->schedule(sparta::Clock::Cycle(0));
    }

    // Called from Retire via in_fetch_flush_redirect_ port
    void Fetch::flushFetch_(const uint64_t & new_addr) {
        ILOG("Fetch: receive flush on new_addr=0x"
             << std::hex << new_addr << std::dec);

        // Cancel all previously sent instructions on the outport
        out_fetch_queue_write_.cancel();

        // No longer speculative
        speculative_path_ = false;
    }

}
