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

        // For flush
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_
             {&unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};

        sparta::UniqueEvent<> ev_rename_insts_ {&unit_event_set_, "rename_insts",
                                                CREATE_SPARTA_HANDLER(Rename, renameInstructions_)};

        const uint32_t num_to_rename_per_cycle_;
        uint32_t credits_dispatch_ = 0;

        // Scoreboards
        using Scoreboards = std::array<sparta::Scoreboard*, core_types::N_REGFILES>;
        Scoreboards scoreboards_;

        //! Rename setup
        void setupRename_();

        //! Free entries from Dispatch
        void creditsDispatchQueue_(const uint32_t &);

        //! Process new instructions coming in from decode
        void decodedInstructions_(const InstGroupPtr &);

        //! Rename instructions
        void renameInstructions_();

        //! Flush instructions.
        void handleFlush_(const FlushManager::FlushingCriteria & criteria);

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
}
