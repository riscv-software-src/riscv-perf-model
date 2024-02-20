// <Fetch.cpp> -*- C++ -*-

//!
//! \file Fetch.cpp
//! \brief Implementation of the CoreModel Fetch unit
//!

#include <algorithm>
#include "Fetch.hpp"
#include "InstGenerator.hpp"
#include "MavisUnit.hpp"
#include "OlympiaAllocators.hpp"

#include "sparta/utils/MathUtils.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "sparta/events/StartupEvent.hpp"

namespace olympia
{
    const char * Fetch::name = "fetch";

    Fetch::Fetch(sparta::TreeNode * node,
                 const FetchParameterSet * p) :
        sparta::Unit(node),
        my_clk_(getClock()),
        num_insts_to_fetch_(p->num_to_fetch),
        skip_nonuser_mode_(p->skip_nonuser_mode),
        icache_block_shift_(sparta::utils::floor_log2(p->block_width.getValue())),
        ibuf_capacity_(std::ceil(p->block_width / 2)), // buffer up instructions read from trace
        target_buffer_("FetchTargetQueue", p->target_queue_size,
                       node->getClock(), &unit_stat_set_),
        fetch_buffer_("FetchBuffer", p->fetch_buffer_size,
                       node->getClock(), &unit_stat_set_),
        memory_access_allocator_(sparta::notNull(OlympiaAllocators::getOlympiaAllocators(node))
                                     ->memory_access_allocator)
    {
        in_fetch_queue_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, receiveFetchQueueCredits_, uint32_t));

        in_fetch_flush_redirect_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, flushFetch_, FlushManager::FlushingCriteria));

        in_icache_fetch_resp_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, receiveCacheResponse_, MemoryAccessInfoPtr));

        in_icache_fetch_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, receiveCacheCredit_, uint32_t));

        ev_predict_insts_.reset(new sparta::SingleCycleUniqueEvent<>(&unit_event_set_, "predict_instructions",
                                                                  CREATE_SPARTA_HANDLER(Fetch, doBranchPrediction_)));

        ev_fetch_insts_.reset(new sparta::SingleCycleUniqueEvent<>(&unit_event_set_, "fetch_instruction_data",
                                                                  CREATE_SPARTA_HANDLER(Fetch, fetchInstruction_)));

        ev_send_insts_.reset(new sparta::SingleCycleUniqueEvent<>(&unit_event_set_, "send_instructions_out",
                                                                  CREATE_SPARTA_HANDLER(Fetch, sendInstructions_)));

        // Schedule a single event to start reading from a trace file
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Fetch, initialize_));

        // Capture when the simulation is stopped prematurely by the ROB i.e. hitting retire limit
        node->getParent()->registerForNotification<bool, Fetch, &Fetch::onROBTerminate_>(
            this, "rob_stopped_notif_channel", false /* ROB maybe not be constructed yet */);

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

        ev_predict_insts_->schedule(1);
    }

    void Fetch::doBranchPrediction_()
    {

        // Prefill the ibuf with some instructions read from the tracefile
        // keeping enough capacity to group them into target blocks
        for (uint32_t i = ibuf_.size(); i < ibuf_capacity_; ++i)
        {
            const auto & inst_ptr = inst_generator_->getNextInst(my_clk_);
            if (SPARTA_EXPECT_TRUE(nullptr != inst_ptr)) {
                ibuf_.emplace_back(inst_ptr);
            }
            else {
                break;
            }
        }

        if (target_buffer_.numFree() == 0 || SPARTA_EXPECT_FALSE(ibuf_.empty())) { return; }

        // // Do a look up in the branch predictor
        // BPredInput lookup(ibuf_.front()->getPC());
        // BPredResult prediction = bpred_->doPrediction(lookup);

        // Find the end of the block
        auto next_block = ibuf_.front()->getPC() + 64; // 64B block sizes
        // auto next_block = ibuf_.front()->getPC() + prediction.getBlockSize();
        auto in_next_block = [next_block](const auto &inst) {
            return (inst->getPC() >= next_block);
        };

        auto block_end = std::find_if(ibuf_.begin(), ibuf_.end(), in_next_block);

        if (disabled_bpred_) {
            // Find the first taken branch
            auto is_taken_branch = [](const auto &inst) { return inst->isTakenBranch(); };
            auto taken_branch = std::find_if(ibuf_.begin(), block_end, is_taken_branch);
            if (taken_branch != block_end) {
                block_end = ++taken_branch;
            }
        }

        // Place instructions into the fetch target queue
        InstGroupPtr target_group = sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);
        for (auto it = ibuf_.begin(); it != block_end; ++it) {
            target_group->emplace_back(*it);
        }
        ibuf_.erase(ibuf_.begin(), block_end);

        // Set up fields used for prediction
        // If prediction is disabled, then just copy the predicted flags.
        auto inst = target_group->back();
        if (disabled_bpred_) {
            if (inst->isTakenBranch()) {
                inst->setPredictedTaken(true);
                inst->setPredictedTarget(inst->getTargetVAddr());
            }
        }

        // At this point, if we hit a taken branch we'd set a flag
        // to create some bubble(s)

        ILOG("predicted target packet: " << target_group);

        // Place packet into fetch target queue
        target_buffer_.push(target_group);
        if (target_buffer_.numFree() > 0) {
            ev_predict_insts_->schedule(1);
        }

        ev_fetch_insts_->schedule(1);
    }

    void Fetch::fetchInstruction_()
    {

        if (credits_icache_ == 0 || target_buffer_.empty() || fetch_buffer_.numFree() == 0) { return; }

        auto target_group = target_buffer_.front();

        // Gather instructions going to the same cacheblock
        // NOTE: This doesn't deal with instructions straddling the blocks,
        // they should be placed into the next group
        auto this_block = target_group->front()->getPC() >> icache_block_shift_;
        auto different_block = [this, this_block](const auto &inst) {
            return (inst->getPC() >> icache_block_shift_) != this_block;
        };

        auto block_end = std::find_if(target_group->begin(), target_group->end(), different_block);

        // TBD we should be able to fetch across fallthrough targets
        InstGroupPtr fetch_group = sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);
        for (auto it = target_group->begin(); it != block_end; it++) {
            fetch_group->emplace_back(*it);
        }
        fetch_buffer_.push(fetch_group);

        // Send to ICache
        auto memory_access_ptr = sparta::allocate_sparta_shared_pointer<MemoryAccessInfo>
            (memory_access_allocator_, fetch_group->front()->getPC());
        memory_access_ptr->setFetchGroup(fetch_group);

        ILOG("requesting: " << fetch_group);

        out_fetch_icache_req_.send(memory_access_ptr);
        --credits_icache_;

        // Pop target buffer
        target_group->erase(target_group->begin(), block_end);
        if (target_group->empty()) {
            target_buffer_.pop();
            ev_predict_insts_->schedule(1);
        }

        if (!target_buffer_.empty() && fetch_buffer_.numFree() > 0 && credits_icache_ > 0) {
            ev_fetch_insts_->schedule(1);
        }
    }

    // Read instructions from the fetch buffer and send them to decode
    void Fetch::sendInstructions_()
    {
        if (fetch_buffer_.empty() || credits_inst_queue_ == 0) { return; }

        auto fetch_group = fetch_buffer_.front();

        // Instructions still waiting for ICache data
        if (fetch_group->front()->getStatus() != Inst::Status::FETCHED) { return; }

        const uint32_t upper = std::min({credits_inst_queue_, num_insts_to_fetch_,
                                         static_cast<uint32_t>(fetch_group->size())});
        sparta_assert(upper > 0);

        InstGroupPtr insts_to_send = sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);
        // TBD we should be able to read across fetch buffer entries
        for (auto it = fetch_group->begin(); it != (fetch_group->begin() + upper); ++it)
        {
            // Send instruction to decode
            const auto inst = *it;
            inst->setSpeculative(speculative_path_);
            insts_to_send->emplace_back(inst);
            ILOG("Sending: " << inst << " down the pipe")
        }

        credits_inst_queue_ -= static_cast<uint32_t>(insts_to_send->size());
        out_fetch_queue_write_.send(insts_to_send);

        // Pop instructions from current fetch group
        fetch_group->erase(fetch_group->begin(), fetch_group->begin() + upper);
        if (fetch_group->empty()) {
            fetch_buffer_.pop();
            ev_fetch_insts_->schedule(1);
        }

        if (!fetch_buffer_.empty() && credits_inst_queue_ > 0) {
            ev_send_insts_->schedule(1);
        }
    }

    void Fetch::receiveCacheResponse_(const MemoryAccessInfoPtr &response)
    {
        const auto & fetched_insts = response->getFetchGroup();
        sparta_assert(fetched_insts != nullptr, "no instructions set for cache request");
        if (response->getCacheState() == MemoryAccessInfo::CacheState::HIT) {
            ILOG("Cache hit response recieved for insts: " << fetched_insts);
            // Mark instructions as fetched
            for(auto & inst : *fetched_insts) {
                inst->setStatus(Inst::Status::FETCHED);
            }
            ev_send_insts_->schedule(sparta::Clock::Cycle(0));
        }

        // Log misses
        if (SPARTA_EXPECT_FALSE(info_logger_) &&
            response->getCacheState() == MemoryAccessInfo::CacheState::MISS) {
            ILOG("Cache miss on insts: " << fetched_insts);
        }
    }

    // Called when ICache has room
    void Fetch::receiveCacheCredit_(const uint32_t &dat)
    {
        credits_icache_ += dat;

        ILOG("Fetch: receive num_credits_icache=" << dat
             << ", total credits_icache=" << credits_icache_);

        // Schedule a fetch event this cycle
        ev_fetch_insts_->schedule(sparta::Clock::Cycle(0));
    }

    // Called when decode has room
    void Fetch::receiveFetchQueueCredits_(const uint32_t & dat) {
        credits_inst_queue_ += dat;

        ILOG("Fetch: receive num_decode_credits=" << dat
             << ", total decode_credits=" << credits_inst_queue_);

        // Schedule a fetch event this cycle
        ev_send_insts_->schedule(sparta::Clock::Cycle(0));
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

        // Cancel any ICache request
        out_fetch_icache_req_.cancel();

        // Clear internal buffers
        ibuf_.clear();
        fetch_buffer_.clear();
        target_buffer_.clear();

        // No longer speculative
        // speculative_path_ = false;
    }

    void Fetch::onROBTerminate_(const bool & stopped)
    {
        rob_stopped_simulation_ = stopped;
    }

    void Fetch::dumpDebugContent_(std::ostream & output) const
    {
        output << "Fetch Buffer Contents" << std::endl;
        for (const auto & entry : fetch_buffer_)
        {
            output << '\t' << entry << std::endl;
        }
    }

    void Fetch::onStartingTeardown_()
    {
        if ((false == rob_stopped_simulation_) && (false == fetch_buffer_.empty()))
        {
            dumpDebugContent_(std::cerr);
            sparta_assert(false, "fetch buffer has pending instructions");
        }
    }

}
