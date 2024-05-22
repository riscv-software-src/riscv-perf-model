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
        my_clk_(getClock()),
        fetched_queue_("FetchedQueue", p->fetched_queue_size, node->getClock(), &unit_stat_set_)
    {
        in_fetch_queue_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, receiveFetchQueueCredits_, uint32_t));

        in_fetch_flush_redirect_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, flushFetch_, FlushManager::FlushingCriteria));

        fetch_inst_event_.reset(new sparta::SingleCycleUniqueEvent<>(&unit_event_set_, "fetch_random",
                                                                     CREATE_SPARTA_HANDLER(Fetch, fetchInstruction_)));
        in_vset_inst_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, process_vset_, InstPtr));
        // Schedule a single event to start reading from a trace file
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Fetch, initialize_));

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
    
    void Fetch::fetchInstruction_()
    {
        const uint32_t upper = std::min(credits_inst_queue_, num_insts_to_fetch_);

        // Nothing to send.  Don't need to schedule this again.
        if(upper == 0) { return; }

        InstGroupPtr insts_to_send = sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);
        for(uint32_t i = 0; i < upper; ++i)
        {
            if(!waiting_on_vset_ || fetched_queue_.size() < fetched_queue_.capacity()){
                // Note -> should we change this to block after the first vector instruction after
                // a vset is detected
                InstPtr ex_inst = nullptr;
                if(fetched_queue_.size() > 0){
                    // if we have already fetched instructions, we should process those first
                    ex_inst = fetched_queue_.read(0);
                    fetched_queue_.pop();
                }
                else{
                    ex_inst = inst_generator_->getNextInst(my_clk_);
                }
                if(SPARTA_EXPECT_TRUE(nullptr != ex_inst)){
                    if ((!waiting_on_vset_) || (waiting_on_vset_ && !ex_inst->isVector()))
                    {
                        // we only need to stall for vset when it's
                        // vsetvl or a vset{i}vl{i} that has a vl that is not the default
                        // any imms can be decoded here and we don't have to stall vset
                        // check if indirect vset
                        // move stalling check to fetch, fetch has to break it up, once one direct vset is detected
                        if(ex_inst->isVset()){
                            if(ex_inst->getSourceOpInfoList()[0].field_value != 0 || ex_inst->getOpCodeInfo()->getInstructionUniqueID() == 315){
                                // vl is being set by register, need to block
                                // vsetvl in mavis -> give it a mavis id, if mavisid == vsetvl number
                                waiting_on_vset_ = true;
                            }
                        }
                        ex_inst->setSpeculative(speculative_path_);
                        insts_to_send->emplace_back(ex_inst);

                        ILOG("Sending: " << ex_inst << " down the pipe");
                    }
                    else{
                        ILOG("Stalling due to waiting on vset");
                        // store fetched instruction in queue
                        fetched_queue_.push((ex_inst));
                        break;
                    }
                }
                else{
                    break;
                }
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

    void Fetch::process_vset_(const InstPtr & inst)
    {
        waiting_on_vset_ = false;
        ILOG("Recieved VSET from ExecutePipe, resuming fetching");
        // schedule fetch, because we've been stalled on vset
        fetch_inst_event_->schedule(sparta::Clock::Cycle(0));
    }
    // Called when decode has room
    void Fetch::receiveFetchQueueCredits_(const uint32_t & dat) {
        credits_inst_queue_ += dat;

        ILOG("Fetch: receive num_decode_credits=" << dat
             << ", total decode_credits=" << credits_inst_queue_);

        // Schedule a fetch event this cycle
        fetch_inst_event_->schedule(sparta::Clock::Cycle(0));
    }

    // Called from FlushManager via in_fetch_flush_redirect_port
    void Fetch::flushFetch_(const FlushManager::FlushingCriteria &criteria)
    {
        ILOG("Fetch: received flush " << criteria);

        auto flush_inst = criteria.getInstPtr();

        // Rewind the tracefile
        if (criteria.isInclusiveFlush())
        {
            inst_generator_->reset(flush_inst, false); // Replay this instruction
        }
        else
        {
            inst_generator_->reset(flush_inst, true); // Skip to next instruction
        }

        // Cancel all previously sent instructions on the outport
        out_fetch_queue_write_.cancel();

        // No longer speculative
        // speculative_path_ = false;
    }

}
