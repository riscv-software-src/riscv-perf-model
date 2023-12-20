// <Rename.h> -*- C++ -*-


#pragma once

#include <string>

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/resources/Scoreboard.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/statistics/Histogram.hpp"
#include "sparta/statistics/BasicHistogram.hpp"


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
            RenameParameterSet(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            { }

            PARAMETER(uint32_t, num_to_rename,       4, "Number of instructions to rename")
            PARAMETER(uint32_t, rename_queue_depth, 10, "Number of instructions queued for rename")
            PARAMETER(uint32_t, num_integer_renames, 128, "Number of integer renames")
            PARAMETER(uint32_t, num_float_renames,   128, "Number of float renames")
        };

        /**
         * @brief Constructor for Rename
         *
         * @param node The node that represents (has a pointer to) the Rename
         * @param p The Rename's parameter set
         */
        Rename(sparta::TreeNode * node,
               const RenameParameterSet * p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static const char name[];

    private:
        InstQueue                         uop_queue_;
        sparta::DataInPort<InstGroupPtr>  in_uop_queue_append_       {&unit_port_set_, "in_uop_queue_append", 1};
        sparta::DataOutPort<uint32_t>     out_uop_queue_credits_     {&unit_port_set_, "out_uop_queue_credits"};
        sparta::DataOutPort<InstGroupPtr> out_dispatch_queue_write_  {&unit_port_set_, "out_dispatch_queue_write"};
        sparta::DataInPort<uint32_t>      in_dispatch_queue_credits_ {&unit_port_set_, "in_dispatch_queue_credits",
                                                                      sparta::SchedulingPhase::Tick, 0};
        sparta::DataInPort<InstPtr>       in_rename_retire_ack_         {&unit_port_set_, "in_rename_retire_ack", 1};

        // For flush
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_
             {&unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};

        sparta::UniqueEvent<> ev_rename_insts_ {&unit_event_set_, "rename_insts",
                                                CREATE_SPARTA_HANDLER(Rename, renameInstructions_)};
        sparta::UniqueEvent<> ev_schedule_rename_ {&unit_event_set_, "schedule_rename",
                                                CREATE_SPARTA_HANDLER(Rename, scheduleRenaming_)};

        const uint32_t num_to_rename_per_cycle_;
        uint32_t num_to_rename_ = 0;
        uint32_t credits_dispatch_ = 0;

        // Scoreboards
        using Scoreboards = std::array<sparta::Scoreboard*, core_types::N_REGFILES>;
        Scoreboards scoreboards_;
        // histogram counter for number of renames each time scheduleRenaming_ is called
        sparta::BasicHistogram<int> rename_histogram_;
        // map of ARF -> PRF
        uint32_t map_table_[core_types::N_REGFILES][32];

        // reference counter for PRF
        std::array<std::vector<int32_t>, core_types::N_REGFILES> reference_counter_;
        // list of free PRF that are available to map
        std::queue<uint32_t> freelist_[core_types::N_REGFILES];
        // used to track current number of each type of RF instruction at each
        // given index in the uop_queue_
        struct RegCountData{
            uint32_t cumulative_reg_counts[core_types::RegFile::N_REGFILES] = {0};
        };
        std::deque<RegCountData> uop_queue_regcount_data_;


        ///////////////////////////////////////////////////////////////////////
        // Stall counters
        enum StallReason {
            NO_DECODE_INSTS,     // No insts from Decode
            NO_DISPATCH_CREDITS, // No credits from Dispatch
            NO_RENAMES,          // Out of renames
            NOT_STALLED,         // Made forward progress (dipatched
                                 // all instructions or no
                                 // instructions)
            N_STALL_REASONS
        };

        StallReason current_stall_ = NO_DECODE_INSTS;
        friend std::ostream&operator<<(std::ostream &, const StallReason &);

        // Counters -- this is only supported in C++11 -- uses
        // Counter's move semantics
        std::array<sparta::CycleCounter, N_STALL_REASONS> stall_counters_{{
                sparta::CycleCounter(getStatisticSet(), "stall_no_decode_insts",
                                     "No Decode Insts",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_no_dispatch_credits",
                                     "No Dispatch Credits",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_no_renames",
                                     "No Renames",
                                     sparta::Counter::COUNT_NORMAL, getClock()),
                sparta::CycleCounter(getStatisticSet(), "stall_not_stalled",
                                     "Rename not stalled, all instructions renamed",
                                     sparta::Counter::COUNT_NORMAL, getClock())
            }};

        //! Rename setup
        void setupRename_();

        //! Free entries from Dispatch
        void creditsDispatchQueue_(const uint32_t &);

        //! Process new instructions coming in from decode
        void decodedInstructions_(const InstGroupPtr &);

        //! Schedule renaming if there are enough PRFs in the freelist_
        void scheduleRenaming_();

        //! Rename instructions
        void renameInstructions_();

        //! Flush instructions.
        void handleFlush_(const FlushManager::FlushingCriteria & criteria);

        // Get Retired Instructions
        void getAckFromROB_(const InstPtr &);
        
        // Friend class used in rename testing
        friend class RenameTester;

    };

    inline std::ostream&operator<<(std::ostream &os, const Rename::StallReason & stall)
    {
        switch(stall)
        {
            case Rename::StallReason::NO_DECODE_INSTS:
                os << "NO_DECODE_INSTS";
                break;
            case Rename::StallReason::NO_DISPATCH_CREDITS:
                os << "NO_DISPATCH_CREDITS";
                break;
            case Rename::StallReason::NO_RENAMES:
                os << "NO_RENAMES";
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
        using ScoreboardFactories = std::array <sparta::ResourceFactory<sparta::Scoreboard,
                                                                        sparta::Scoreboard::ScoreboardParameters>,
                                                core_types::RegFile::N_REGFILES>;
        ScoreboardFactories sb_facts_;
        ScoreboardTreeNodes sb_tns_;
    };
    class RenameTester;
}
