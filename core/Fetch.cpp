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

        in_fetch_flush_redirect_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, flushFetch_, InstPtr));

        in_rob_retire_ack_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, getAckFromROB_, InstPtr));

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

        // Setup BTB
        btb_ = getContainer()->getChild("btb")->getResourceAs<olympia::BTB>();

        fetch_inst_event_->schedule(1);
    }


    bool Fetch::predictInstruction_(const InstPtr & inst)
    {
        if (btb_->isHit(inst->getPC()))
        {
            ++btb_hits_;

            inst->setBTBHit(true);

            auto btb_entry = btb_->getLine(inst->getPC());
            btb_->touchMRU(*btb_entry);

            auto predicted_taken = btb_entry->predictDirection();
            inst->setBranchMispredict(predicted_taken != inst->isTakenBranch());

            // TODO Compare targets (RAS/Indirect mispredicitons)
            // TODO Consider BTB hit aliasing..

            ILOG("BTB Hit on " << inst << " predicted " << (predicted_taken ? "taken" : "not-taken"));

            return predicted_taken;
        }
        return false;
    }

    void Fetch::getAckFromROB_(const InstPtr & inst)
    {
        // If we've mispredicted the branch, then the flush should have happened already
        // so the branch should have been inserted in the BTB already. Just update the direction
        // counter.
        if (inst->isBranch() && inst->isCondBranch() && btb_->isHit(inst->getPC()))
        {
            auto btb_entry = btb_->getLine(inst->getPC());
            btb_entry->updateDirection(inst->isTakenBranch());
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
                ILOG("Redirection predicted, ending fetch group")
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

    // Could be a flush from decode, or retirement
    // Decode flush -> BTB miss
    // Retire flush -> Mispredicted branch (and potentially a BTB miss)
    void Fetch::flushFetch_(const InstPtr & flush_inst)
    {
        ILOG("Fetch: receive flush " << flush_inst);

        // Insert BTB misses on flush
        auto pc = flush_inst->getPC();
        if (flush_inst->isBranch() && !btb_->isHit(pc))
        {
            ++btb_misses_;

            ILOG("Allocating miss into BTB: " << flush_inst);

            auto entry = btb_->getLineForReplacementWithInvalidCheck(pc);
            entry->setValid(true);
            entry->setAddr(pc);
            entry->setTarget(flush_inst->getTargetVAddr());
            if (flush_inst->isCondBranch())
            {
                entry->setBranchType(BTBEntry::BranchType::CONDITIONAL);
            }
            else
            {
                entry->setBranchType(BTBEntry::BranchType::DIRECT);
            }
            btb_->touchMRU(*entry);
        }

        // Rewind the tracefile
        if (flush_inst->getStatus() == Inst::Status::COMPLETED || flush_inst->getStatus() == Inst::Status::RETIRED)
        {
            inst_generator_->reset(flush_inst, true); // Skip to next instruction
        }
        else
        {
            inst_generator_->reset(flush_inst, false); // Replay this instruction
        }

        // Cancel all previously sent instructions on the outport
        out_fetch_queue_write_.cancel();

        // No longer speculative
        // speculative_path_ = false;
    }


    void FetchFactory::onConfiguring(sparta::ResourceTreeNode* node)
    {
        btb_tn_.reset(new sparta::ResourceTreeNode(node,
                                                   "btb",
                                                   sparta::TreeNode::GROUP_NAME_NONE,
                                                   sparta::TreeNode::GROUP_IDX_NONE,
                                                   "Branch Target Buffer",
                                                   &btb_fact_));
    }

    void FetchFactory::deleteSubtree(sparta::ResourceTreeNode*) {
        btb_tn_.reset();
    }

}
