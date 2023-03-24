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
        ~Rename(){
            // delete map_table and reference counter, as they are dynamically allocated
            map_table_.reset();
            delete [] reference_counter_;
        }

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
        uint32_t credits_dispatch_ = 0;

        // Scoreboards
        using Scoreboards = std::array<sparta::Scoreboard*, core_types::N_REGFILES>;
        Scoreboards scoreboards_;

        // (data value, valid bit)
        using MapPair = std::pair<uint32_t, bool>;
        // map of ARF -> PRF
        std::unique_ptr<std::unique_ptr<MapPair[]>[]> map_table_ = std::make_unique<std::unique_ptr<MapPair[]>[]>(core_types::N_REGFILES);
        // reference counter for PRF
        std::unique_ptr<int32_t[]> * reference_counter_ = new std::unique_ptr<int32_t[]>[core_types::N_REGFILES];
        // list of free PRF that are available to map
        std::queue<uint32_t> freelist_[core_types::N_REGFILES];

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

        friend class RenameTester;

    };

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

    // Rename Tester Class
    class RenameTester
    {
    public:
        void test_clearing_rename_structures(Rename & rename){
            // after all instructions have retired, we should have a full freelist
            EXPECT_TRUE(rename.freelist_[0].size() == 128);
            EXPECT_TRUE(rename.reference_counter_[0][1] == 0);
            EXPECT_TRUE(rename.reference_counter_[0][2] == 0);

            // spot checking architectural registers mappings are cleared and set to invalid
            // as mappings are cleared once an instruction are retired
            EXPECT_TRUE(rename.map_table_[0][0].second == false);
            EXPECT_TRUE(rename.map_table_[0][3].second == false);
            EXPECT_TRUE(rename.map_table_[0][4].second == false);
            EXPECT_TRUE(rename.map_table_[0][5].second == false);
            EXPECT_TRUE(rename.map_table_[0][64].second == false);
            EXPECT_TRUE(rename.map_table_[0][125].second == false);
        }
        void test_one_instruction(Rename & rename){
            // process only one instruction, check that freelist and map_tables are allocated correctly
            EXPECT_TRUE(rename.freelist_[0].size() == 127);
            // map table entry is valid, as it's been allocated
            EXPECT_TRUE(rename.map_table_[0][3].second == true);
            
            // reference counters should be 0, because the SRCs aren't referencing any PRFs
            EXPECT_TRUE(rename.reference_counter_[0][1] == 0);
            EXPECT_TRUE(rename.reference_counter_[0][2] == 0);
        }
        void test_multiple_instructions(Rename & rename){
            // first two instructions are RAW
            // so the second instruction should increase reference count
            EXPECT_TRUE(rename.reference_counter_[0][0] == 1);
        }
    };
}
