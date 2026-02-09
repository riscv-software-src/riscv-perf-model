// <Fetch.cpp> -*- C++ -*-

//!
//! \file Fetch.cpp
//! \brief Implementation of the CoreModel Fetch unit
//!

#include <algorithm>
#include "fetch/Fetch.hpp"
#include "InstGenerator.hpp"
#include "decode/MavisUnit.hpp"
#include "OlympiaAllocators.hpp"

#include "sparta/utils/MathUtils.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "sparta/events/StartupEvent.hpp"

namespace olympia
{
    const char* Fetch::name = "fetch";

    Fetch::Fetch(sparta::TreeNode* node, const FetchParameterSet* p) :
        sparta::Unit(node),
        my_clk_(getClock()),
        num_insts_to_fetch_(p->num_to_fetch),
        skip_nonuser_mode_(p->skip_nonuser_mode),
        icache_block_shift_(sparta::utils::floor_log2(p->block_width.getValue())),
        ibuf_capacity_(std::ceil(p->block_width / 2)), // buffer up instructions read from trace
        fetch_buffer_capacity_(p->fetch_buffer_size),
        memory_access_allocator_(sparta::notNull(OlympiaAllocators::getOlympiaAllocators(node))
                                     ->memory_access_allocator)
    {
        in_fetch_queue_credits_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, receiveFetchQueueCredits_, uint32_t));

        in_fetch_flush_redirect_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, flushFetch_, FlushManager::FlushingCriteria));

        in_icache_fetch_resp_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, receiveCacheResponse_, MemoryAccessInfoPtr));

        in_icache_fetch_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, receiveCacheCredit_, uint32_t));

        ev_fetch_insts.reset(new sparta::SingleCycleUniqueEvent<>(&unit_event_set_, "fetch_instruction_data",
                                                                  CREATE_SPARTA_HANDLER(Fetch, fetchInstruction_)));

        ev_send_insts.reset(new sparta::SingleCycleUniqueEvent<>(&unit_event_set_, "send_instructions_out",
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
        inst_generator_ = InstGenerator::createGenerator(info_logger_,
                                                         getMavis(getContainer()),
                                                         workload->getValueAsString(),
                                                         skip_nonuser_mode_);

        ev_fetch_insts->schedule(1);
    }


    void Fetch::fetchInstruction_()
    {
        // Prefill the ibuf with some instructions read from the tracefile
        // keeping enough capacity to group them into cache block accesses.
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

        if (credits_icache_ == 0 || ibuf_.empty() || fetch_buffer_.size() > fetch_buffer_capacity_) { return; }

        // Gather instructions going to the same cacheblock
        // NOTE: This doesn't deal with instructions straddling the blocks,
        // they should be placed into the next group
        auto different_blocks = [this](const auto &lhs, const auto &rhs) {
            return (lhs->getPC() >> icache_block_shift_) != (rhs->getPC() >> icache_block_shift_) ||
                    lhs->isTakenBranch() ||
                    rhs->isCoF();
        };

        auto block_end = std::adjacent_find(ibuf_.begin(), ibuf_.end(), different_blocks);
        if (block_end != ibuf_.end()) {
            ++block_end;
        }

        // Send to ICache
        auto memory_access_ptr = sparta::allocate_sparta_shared_pointer<MemoryAccessInfo>(memory_access_allocator_,
                                                                    ibuf_.front()->getPC());

        InstGroupPtr fetch_group_ptr = sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);

        // Place in fetch group for the memory access, and place in fetch buffer for later processing.
        for (auto iter = ibuf_.begin(); iter != block_end; iter++) {
            fetch_group_ptr->emplace_back(*iter);
            fetch_buffer_.emplace_back(*iter);
        }

        // Set the last in block
        fetch_buffer_.back()->setLastInFetchBlock(true);

        // Associate the icache transaction with the instructions
        memory_access_ptr->setFetchGroup(fetch_group_ptr);

        ILOG("requesting: " << fetch_group_ptr);

        out_fetch_icache_req_.send(memory_access_ptr);
        --credits_icache_;

        // We want to track blocks, not instructions.
        ++fetch_buffer_occupancy_;

        ibuf_.erase(ibuf_.begin(), block_end);
        if (!ibuf_.empty() && credits_icache_ > 0 && fetch_buffer_occupancy_ < fetch_buffer_capacity_) {
            ev_fetch_insts->schedule(1);
        }
    }

    // Read instructions from the fetch buffer and send them to decode
    void Fetch::sendInstructions_()
    {
        const uint32_t upper = std::min({credits_inst_queue_, num_insts_to_fetch_,
                                         static_cast<uint32_t>(fetch_buffer_.size())});

        // Nothing to send.  Don't need to schedule this again.
        if (upper == 0) { return ; }

        InstGroupPtr insts_to_send = sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);
        for (uint32_t i = 0; i < upper; ++i)
        {
            const auto entry = fetch_buffer_.front();

            // Can't send instructions that still waiting for ICache data
            if (entry->getStatus() != Inst::Status::FETCHED) {
                break;
            }

            // Don't group instructions where there has been a change of flow
            if (entry->isCoF() && insts_to_send->size() > 0) {
                break;
            }

            // Send instruction to decode
            entry->setSpeculative(speculative_path_);
            insts_to_send->emplace_back(entry);
            ILOG("Sending: " << entry << " down the pipe")
            fetch_buffer_.pop_front();

            if (entry->isLastInFetchBlock()) {
                --fetch_buffer_occupancy_;
            }

            // Only one taken branch per group
            if (entry->isTakenBranch()) {
                break;
            }
        }

        credits_inst_queue_ -= static_cast<uint32_t>(insts_to_send->size());
        out_fetch_queue_write_.send(insts_to_send);

        if (!fetch_buffer_.empty() && credits_inst_queue_ > 0) {
            ev_send_insts->schedule(1);
        }

        ev_fetch_insts->schedule(1);

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
            ev_send_insts->schedule(sparta::Clock::Cycle(0));
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
        ev_fetch_insts->schedule(sparta::Clock::Cycle(0));
    }

    // Called when decode has room
    void Fetch::receiveFetchQueueCredits_(const uint32_t & dat)
    {
        credits_inst_queue_ += dat;

        ILOG("Fetch: receive num_decode_credits=" << dat
             << ", total decode_credits=" << credits_inst_queue_);

        // Schedule a fetch event this cycle
        ev_send_insts->schedule(sparta::Clock::Cycle(0));
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

        // No longer speculative
        // speculative_path_ = false;

	// Fix for Issue #254
	// It is possible that we do not have any external trigger to restart
	// fetch, so force bootstrap it like when initialized
	ev_fetch_insts->schedule(1);
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
