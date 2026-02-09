// <Rename.hpp> -*- C++ -*-

/**
 * @file   Rename.hpp
 * @brief
 *
 * Rename will
 * 1. Create the rename uop queue
 * 2. Rename the uops and send to dispatch pipe (retrieved via port)
 * 3. The dispatch pipe will send to unit for schedule
 */

#pragma once

#include <string>

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/statistics/Histogram.hpp"
#include "sparta/statistics/BasicHistogram.hpp"
#include "sparta/pevents/PeventCollector.hpp"
#include "sparta/resources/Scoreboard.hpp"

#include "CoreTypes.hpp"
#include "FlushManager.hpp"
#include "InstGroup.hpp"

namespace olympia
{

    /**
     * @file   Rename.h
     * @brief
     *
     * Rename will
     * 1. Create the rename uop queue
     * 2. Rename the uops and send to dispatch pipe (retrieved via port)
     * 3. The dispatch pipe will send to unit for schedule
     */
    class Rename : public sparta::Unit
    {
      public:
        //! \brief Parameters for Rename model
        class RenameParameterSet : public sparta::ParameterSet
        {
          public:
            RenameParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

            PARAMETER(uint32_t, num_to_rename, 4, "Number of instructions to rename")
            PARAMETER(uint32_t, rename_queue_depth, 10, "Number of instructions queued for rename")
            PARAMETER(uint32_t, num_integer_renames, 128, "Number of integer renames")
            PARAMETER(uint32_t, num_float_renames, 128, "Number of float renames")
            PARAMETER(uint32_t, num_vector_renames, 128, "Number of vector renames")
            PARAMETER(bool, partial_rename, true,
                      "Rename all or partial instructions in a received group")
            PARAMETER(bool, move_elimination, false, "Enable move elimination")
        };

        /**
         * @brief Constructor for Rename
         *
         * @param node The node that represents (has a pointer to) the Rename
         * @param p The Rename's parameter set
         */
        Rename(sparta::TreeNode* node, const RenameParameterSet* p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static const char name[];

      private:
        static constexpr uint32_t NUM_RISCV_REGS_ = 32; // default risc-v ARF count

        const uint32_t num_to_rename_per_cycle_;
        const bool partial_rename_;
        const bool enable_move_elimination_;

        sparta::DataInPort<InstGroupPtr> in_uop_queue_append_{&unit_port_set_,
                                                              "in_uop_queue_append", 1};
        sparta::DataOutPort<uint32_t> out_uop_queue_credits_{&unit_port_set_,
                                                             "out_uop_queue_credits"};
        sparta::DataOutPort<InstGroupPtr> out_dispatch_queue_write_{&unit_port_set_,
                                                                    "out_dispatch_queue_write"};
        sparta::DataInPort<uint32_t> in_dispatch_queue_credits_{
            &unit_port_set_, "in_dispatch_queue_credits", sparta::SchedulingPhase::Tick, 0};
        sparta::DataInPort<InstGroupPtr> in_rename_retire_ack_{&unit_port_set_,
                                                               "in_rename_retire_ack", 1};

        // For flush
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_{
            &unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};

        sparta::UniqueEvent<> ev_rename_insts_{&unit_event_set_, "rename_insts",
                                               CREATE_SPARTA_HANDLER(Rename, renameInstructions_)};

        sparta::UniqueEvent<> ev_debug_rename_{
            &unit_event_set_, "debug_rename",
            CREATE_SPARTA_HANDLER(Rename, dumpDebugContentHearbeat_)};

        sparta::UniqueEvent<> ev_schedule_rename_{&unit_event_set_, "schedule_rename",
                                                  CREATE_SPARTA_HANDLER(Rename, scheduleRenaming_)};

        sparta::Event<> ev_sanity_check_{&unit_event_set_, "ev_sanity_check",
                                         CREATE_SPARTA_HANDLER(Rename, sanityCheck_)};

        // histogram counter for number of renames each time scheduleRenaming_ is called
        sparta::BasicHistogram<int> rename_histogram_;

        // reference counter for PRF
        struct Producer
        {
            Producer(uint32_t _cnt) : cnt(_cnt) {}

            uint32_t cnt = 0;
            uint64_t producer_id = 0;
            InstWeakPtr producer;
            bool producer_is_load = false;
        };

        struct RegfileComponents
        {
            // Scoreboard
            sparta::Scoreboard* scoreboard = nullptr;

            // reference counter for PRF
            std::vector<Producer> reference_counter;

            // list of free PRF that are available to map
            std::queue<uint32_t> freelist;
        };

        using RegfileComponentArray = std::vector<RegfileComponents>;
        RegfileComponentArray regfile_components_{core_types::RegFile::N_REGFILES};

        void updateRegcountData_();

        // RENAME (Decode Mapping) event
        sparta::pevents::PeventCollector<InstPEventPairs> rename_event_;

        // Counters
        sparta::Counter move_eliminations_{getStatisticSet(), "move_eliminations",
                                           "Number of times Rename eliminated a move instruction",
                                           sparta::Counter::COUNT_NORMAL};

        ///////////////////////////////////////////////////////////////////////
        // Stall counters
        enum StallReason
        {
            NO_DECODE_INSTS,     // No insts from Decode
            NO_DISPATCH_CREDITS, // No credits from Dispatch
            NO_INTEGER_RENAMES,  // Out of integer renames
            NO_FLOAT_RENAMES,    // Out of float renames
            NO_VECTOR_RENAMES,   // Out of vector renames
            NOT_STALLED,         // Made forward progress (dipatched
            // all instructions or no
            // instructions)
            N_STALL_REASONS
        };

        friend std::ostream & operator<<(std::ostream &, const StallReason &);

        // Counters -- this is only supported in C++11 -- uses
        // Counter's move semantics
        std::array<sparta::CycleCounter, N_STALL_REASONS> stall_counters_{
            {sparta::CycleCounter(getStatisticSet(), "stall_no_decode_insts", "No Decode Insts",
                                  sparta::Counter::COUNT_NORMAL, getClock()),
             sparta::CycleCounter(getStatisticSet(), "stall_no_dispatch_credits",
                                  "No Dispatch Credits", sparta::Counter::COUNT_NORMAL, getClock()),
             sparta::CycleCounter(getStatisticSet(), "stall_no_integer_renames",
                                  "No Integer Renames", sparta::Counter::COUNT_NORMAL, getClock()),
             sparta::CycleCounter(getStatisticSet(), "stall_no_float_renames", "No Float Renames",
                                  sparta::Counter::COUNT_NORMAL, getClock()),
             sparta::CycleCounter(getStatisticSet(), "stall_no_vector_renames", "No Vector Renames",
                                  sparta::Counter::COUNT_NORMAL, getClock()),
             sparta::CycleCounter(getStatisticSet(), "stall_not_stalled",
                                  "Rename not stalled, all instructions renamed",
                                  sparta::Counter::COUNT_NORMAL, getClock())}};

        // used to track current number of each type of RF instruction at each
        // given index in the uop_queue
        struct RegCountData
        {
            uint32_t cumulative_reg_counts[core_types::RegFile::N_REGFILES] = {0};
        };

        // This is ordered roughly from most-accessed ->
        // least-accessed, and members that are frequently accessed
        // together are next to each other
        uint32_t credits_dispatch_ = 0;
        InstBuffer uop_queue_;
        RegCountData uop_queue_regcount_data_;

        // map of ARF -> PRF
        std::array<std::array<uint32_t, NUM_RISCV_REGS_>, core_types::N_REGFILES> map_table_;

        // Used to track inflight instructions for the purpose of recovering
        // the rename data structures
        std::deque<InstPtr> inst_queue_;
        StallReason current_stall_ = StallReason::NO_DECODE_INSTS;

        void setStall_(const StallReason reason)
        {
            stall_counters_[current_stall_].stopCounting();
            current_stall_ = reason;
            stall_counters_[current_stall_].startCounting();
        }

        StallReason getStall_() const { return current_stall_; }

        //! Rename setup
        void setupRename_();

        //! Free entries from Dispatch
        void creditsDispatchQueue_(const uint32_t &);

        //! Process new instructions coming in from decode
        void decodedInstructions_(const InstGroupPtr &);

        //! Can the oldest instruction secure a rename?
        std::pair<bool, Rename::StallReason> enoughRenames_() const;

        //! Schedule renaming if there are enough PRFs in the freelist_
        void scheduleRenaming_();

        //! Rename instructions
        void renameInstructions_();

        //! Rename the sources
        void renameSources_(const InstPtr &);

        //! Rename the dests
        void renameDests_(const InstPtr &);

        //! Flush instructions.
        void handleFlush_(const FlushManager::FlushingCriteria & criteria);

        // Get Retired Instructions
        void getAckFromROB_(const InstGroupPtr &);

        // Friend class used in rename testing
        friend class RenameTester;

        // Sanity checker
        void sanityCheck_();

        // Dump debug content on failure
        void dumpDebugContentHearbeat_();
        template <class OStreamT> void dumpRenameContent_(OStreamT & output) const;
        void dumpDebugContent_(std::ostream & output) const override final;
    };

    inline std::ostream & operator<<(std::ostream & os, const Rename::StallReason & stall)
    {
        switch (stall)
        {
        case Rename::StallReason::NO_DECODE_INSTS:
            os << "NO_DECODE_INSTS";
            break;
        case Rename::StallReason::NO_DISPATCH_CREDITS:
            os << "NO_DISPATCH_CREDITS";
            break;
        case Rename::StallReason::NO_INTEGER_RENAMES:
            os << "NO_INTEGER_RENAMES";
            break;
        case Rename::StallReason::NO_FLOAT_RENAMES:
            os << "NO_FLOAT_RENAMES";
            break;
        case Rename::StallReason::NO_VECTOR_RENAMES:
            os << "NO_VECTOR_RENAMES";
            break;
        case Rename::StallReason::NOT_STALLED:
            os << "NOT_STALLED";
            break;
        case Rename::StallReason::N_STALL_REASONS:
            sparta_assert(false, "How'd we get here?");
        }
        return os;
    }

    //! Rename's factory class. Don't create Rename without it
    class RenameFactory : public sparta::ResourceFactory<Rename, Rename::RenameParameterSet>
    {
      public:
        void onConfiguring(sparta::ResourceTreeNode* node) override;

      private:
        using ScoreboardTreeNodes = std::vector<std::unique_ptr<sparta::TreeNode>>;
        using ScoreboardFactories = std::array<
            sparta::ResourceFactory<sparta::Scoreboard, sparta::Scoreboard::ScoreboardParameters>,
            core_types::RegFile::N_REGFILES>;
        ScoreboardFactories sb_facts_;
        ScoreboardTreeNodes sb_tns_;
    };

    class RenameTester;
} // namespace olympia
