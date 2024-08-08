// <Decode.h> -*- C++ -*-
//! \file Decode.hpp
#pragma once

#include "CoreTypes.hpp"
#include "FlushManager.hpp"
#include "InstGroup.hpp"
#include "MavisUnit.hpp"

#include "fsl_api/FieldExtractor.h"
#include "fsl_api/Fusion.h"
#include "fsl_api/FusionGroup.h"
#include "fsl_api/FusionTypes.h"
#include "fsl_api/HCache.h"
#include "fsl_api/MachineInfo.h"

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace olympia
{
    class VectorUopGenerator;
    /**
     * @file   Decode.h
     * @brief Decode instructions from Fetch and send them on
     *
     * Decode unit will
     * 1. Retrieve instructions from the fetch queue (retrieved via port)
     * 2. Push the instruction down the decode pipe (internal, of parameterized length)
     */
    class Decode : public sparta::Unit
    {

        //! \brief ...
        using FusionGroupType = fusion::FusionGroup<MachineInfo, FieldExtractor>;
        //! \brief ...
        using FusionType = fusion::Fusion<FusionGroupType, MachineInfo, FieldExtractor>;
        //! \brief ...
        using FusionGroupListType = std::vector<FusionGroupType>;
        //! \brief ...
        using FusionGroupContainerType = std::unordered_map<fusion::HashType, FusionGroupType>;
        //! \brief ...
        using FusionGroupCfgType = fusion::FusionGroupCfg<MachineInfo, FieldExtractor>;
        //! \brief ...
        using FusionGroupCfgListType = std::vector<FusionGroupCfgType>;
        //! \brief ...
        using InstUidListType = fusion::InstUidListType;
        //! \brief the is the return reference type for group matches
        using MatchInfoListType = std::vector<fusion::FusionGroupMatchInfo>;
        //! \brief ...
        using HashPair = pair<size_t, fusion::HashType>;
        //! \brief ...
        using HashPairListType = std::vector<HashPair>;
        //! \brief ...
        using FileNameListType = fusion::FileNameListType;

      public:
        //! \brief Parameters for Decode model
        class DecodeParameterSet : public sparta::ParameterSet
        {
          public:
            DecodeParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

            //! \brief width of decoder
            PARAMETER(uint32_t, num_to_decode, 4, "Decode group size")

            //! \brief depth of the input instruction buffer
            PARAMETER(uint32_t, fetch_queue_size, 10, "Size of the fetch queue")

            //! \brief enable fusion operations
            //!
            //! master enable, when false fusion_* parmeters have no effect
            PARAMETER(bool, fusion_enable, false, "enable the fusion logic")

            //! \brief enable fusion specific debug statements
            PARAMETER(bool, fusion_debug, false, "enable fusion debug logic")

            //! \brief fusion group enable register
            //!
            //! This is a bit mask that enables each fusion transform
            //! Rather than a series of parameters for each fusion group
            //! this uses one bit per fusion group. Default is all on.
            //!
            //! \see fusion_enable_register_ for the encoding
            //! in the yaml to choose a transform my name
            PARAMETER(uint32_t, fusion_enable_register, std::numeric_limits<uint32_t>::max(),
                      "bit-wise fusion group enable")

            //! \brief max acceptable latency created by gathering uops
            //!
            //! Depending on the max fusion tuple size uops will be gathered,
            //! when downstream is not busy this introduces some latency.
            //! This knob allows studying the effects. fusion_enabled must
            //! be true
            PARAMETER(uint32_t, fusion_max_latency, 4, "max fusion latency")

            //! \brief fusion group match max iterations
            PARAMETER(uint32_t, fusion_match_max_tries, 1023, "fusion group match attempts limit")

            //! \brief records the largest fusion group size
            //!
            //! Used with fusion_max_latency to gather instructions for
            //! possible fusion. If number of decode is >= fusion group size
            //! there is no need to continue to gather instructions.
            PARAMETER(uint32_t, fusion_max_group_size, 4, "max fusion group size")

            //! \brief ...
            PARAMETER(std::string, fusion_summary_report, "fusion_summary.txt",
                      "Path to fusion report file")

            //! \brief ...
            PARAMETER(FileNameListType, fusion_group_definitions, {},
                      "Lists of fusion group UID json files")

            //! LMUL
            PARAMETER(uint32_t, init_lmul, 1, "effective length")

            //! Element width in bits
            PARAMETER(uint32_t, init_sew, 8, "element width")

            //! Vector length, number of elements
            PARAMETER(uint32_t, init_vl, 128, "vector length")

            //! Vector tail agnostic, default is undisturbed
            PARAMETER(bool, init_vta, 0, "vector tail agnostic")
        };

        /**
         * @brief Constructor for Decode
         *
         * @param node The node that represents (has a pointer to) the Decode
         * @param p The Decode's parameter set
         */
        Decode(sparta::TreeNode* node, const DecodeParameterSet* p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static constexpr char name[] = "decode";

      private:
        // The internal instruction queue
        InstQueue fetch_queue_;

        // Vector uop generator
        VectorUopGenerator * vec_uop_gen_ = nullptr;

        // Port listening to the fetch queue appends - Note the 1 cycle delay
        sparta::DataInPort<InstGroupPtr> fetch_queue_write_in_{&unit_port_set_,
                                                               "in_fetch_queue_write", 1};
        sparta::DataInPort<InstPtr> in_vset_inst_{&unit_port_set_, "in_vset_inst", 1};
        sparta::DataOutPort<uint32_t> fetch_queue_credits_outp_{&unit_port_set_,
                                                                "out_fetch_queue_credits"};

        // Port to the uop queue in dispatch (output and credits)
        sparta::DataOutPort<InstGroupPtr> uop_queue_outp_{&unit_port_set_, "out_uop_queue_write"};
        sparta::DataInPort<uint32_t> uop_queue_credits_in_{&unit_port_set_, "in_uop_queue_credits",
                                                           sparta::SchedulingPhase::Tick, 0};

        // For flush
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_{
            &unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};

        //! \brief decode instruction event
        sparta::UniqueEvent<> ev_decode_insts_event_{&unit_event_set_, "decode_insts_event",
                                                     CREATE_SPARTA_HANDLER(Decode, decodeInsts_)};

        //! \brief initialize the fusion group configuration
        void constructFusionGroups_();

        //! \brief compare the dynamic uid vector to the predefined groups
        //!
        //! returns a sorted list of matches
        //! Matches the fusionGroup sets against the input. There is a continuation
        //! check invoked if the fusionGroup is larger than the input list.
        //! FusionGroups must exactly match a segment of the input.
        //!
        //! During construction each fusion group has a pre-calculated Hash
        //! created from the UIDs in the group.
        //!
        //! The group hash is matched against hashes formed from the input UIDs.
        //!
        //! In order to match the input to the group, hashes must be created of
        //! the input.
        //!
        //! A 'cache' of the inputUids is created on entry. The cache is indexed
        //! by group size. The cache data is a vector of pairs, pair.first is the
        //! input position within inputUids, pair.second is the hash of the
        //! inputUids in that segment.
        void matchFusionGroups_(MatchInfoListType & matches, InstGroupPtr & insts,
                                InstUidListType & inputUIDS, FusionGroupContainerType &);

        //! \brief process the fusion matches
        void processMatches_(MatchInfoListType &, InstGroupPtr & insts,
                             const InstUidListType & inputUIDS);

        //! \brief Remove the ghost ops
        //!
        //! The matching pass identifies the master fusion op (FUSED)
        //! and the ops that will be eliminated (FUSION_GHOST).
        //! This pass removes the ghosts. Future feature: not currently used.
        void whereIsEgon_(InstGroupPtr &, uint32_t &);

        //! \brief ...
        void infoInsts_(std::ostream & os, const InstGroupPtr & insts);

        //! \brief initialize the fusion api structures
        //!
        //! This can construct the FusionGroup lists using multiple
        //! methods, as static objects, as objects read from files, etc.
        //!
        //! This called a small number of support methods.
        void initializeFusion_();

        //! \brief assign the current fusion group configs to a named context
        //!
        //! The Fusion API supports multiple contexts. There is a single
        //! context in CAM at the moment.
        //!
        //! This method constructs the context and throws if there are
        //! problems detected during construction
        void setFusionContext_(FusionGroupListType &);

        //! \brief fusion groups are contructed statically or from DSL files
        void constructGroups_(const FusionGroupCfgListType &, FusionGroupListType &);

        //! \brief record stats for fusionGroup name
        void updateFusionGroupUtilization_(std::string name);

        //! \brief number of custom instructions created for fusion
        sparta::Counter fusion_num_fuse_instructions_;

        //! \brief number of instructions eliminated by fusion
        sparta::Counter fusion_num_ghost_instructions_;

        //! \brief the number of entries in the fusion group struct
        //!
        //! This is a run-time determined value extracted from parsing
        //! the JSON files. Future feature: FSL file support
        sparta::Counter fusion_num_groups_defined_;

        //! \brief the number of fusion groups utilizaed
        //!
        //! Some number of run-time or compile-time fusion groups
        //! are defined.
        //!
        //! This is a measure of how many defined groups were
        //! used during execution. A fusion group used 100 times
        //! counts only once.
        sparta::Counter fusion_num_groups_utilized_;

        //! \brief the predicted number of cycles saved by fusion
        //!
        //! For the simplist case a fusion group of N instructions
        //! saves N-1 cycles. Modified for the cases where some
        //! instructions and some fusion groups are multi-cycle.
        //!
        //! This is a prediction without concern for pipeline
        //! behavior. This is compared against measured cycles
        //! with fusion enabled vs. disabled to show effectiveness
        //! of the defined fusion groups.
        sparta::Counter fusion_pred_cycles_saved_;

        //! \brief temporary for number of instructions to decode this time
        const uint32_t num_to_decode_;

        //! \brief master fusion enable data member
        //!
        //! This is the model's fusion enable parameter. When not enabled
        //! the fusion methods have no impact on function or performance
        //! of the model. \see fusion_enable_register_;
        const bool fusion_enable_{false};

        //! \brief fusion debug switch setting
        //!
        //! There are infrequent debug related statements that are gated
        //! with this parameter.
        const bool fusion_debug_{false};

        //! \brief represents the FER in h/w
        //!
        //! The register holds an encoded value. Each fusion group
        //! has an associated page, and an enable. The upper 4 bits
        //! represent a page, the lower 28 bits are the enables.
        //! PAGE=0xF all enabled, PAGE=0x0 all disabled. This is preferred
        //! over a separate page register and enable register.
        //!
        //! Currently defined but not used
        const uint32_t fusion_enable_register_{0};

        //! \brief maximum allowed latency induced by fusion
        //!
        //! This typically zero, a parameter is provided to allow
        //! experimenting with instruction gathering
        const uint32_t fusion_max_latency_{0};

        //! \brief max attempts to match UIDs of instrs in the XIQ
        const uint32_t fusion_match_max_tries_{0};

        //! \brief the longest chain of UIDs across all groups
        //!
        //! \see FusionGroup
        uint32_t fusion_max_group_size_{0};

        //! \brief running count of fusion latency
        uint32_t latency_count_{0};

        //! \brief the Fusion API instance
        //!
        //! \see Fusion.hpp
        std::unique_ptr<FusionType> fuser_{nullptr};

        //! \brief fusion group matching hash
        fusion::HCache hcache_;

        //! \brief fusion function object callback proxies
        struct cbProxy_;
        //! \brief this counts the number of times a group is used
        std::map<std::string, uint32_t> matchedFusionGroups_;
        //! \brief fusion specific report file name
        const std::string fusion_summary_report_;
        //! \brief the fusion group definition files, JSON or (future) FSL
        const std::vector<std::string> fusion_group_definitions_;

        //////////////////////////////////////////////////////////////////////
        // Vector
        const bool vector_enabled_;
        VectorConfigPtr vector_config_;

        bool waiting_on_vset_;

        // Helper method to get the current vector config
        void updateVectorConfig_(const InstPtr &);

        uint32_t getNumVecUopsRemaining() const;

        //////////////////////////////////////////////////////////////////////
        // Decoder callbacks
        void sendInitialCredits_();
        void fetchBufferAppended_(const InstGroupPtr &);
        void receiveUopQueueCredits_(const uint32_t &);
        void process_vset_(const InstPtr &);
        void decodeInsts_();
        void handleFlush_(const FlushManager::FlushingCriteria & criteria);

        uint32_t uop_queue_credits_ = 0;
        friend class DecodeTester;
    };
    class DecodeTester;

    //! \brief the fusion functor/function objects
    //!
    //! there is one function object assigned to each group,
    //! the objects can be shared across groups
    struct Decode::cbProxy_
    {
        //! \brief this is the default 'no fusion' operation
        //!
        //! false indicates no transform was performed
        static bool dfltXform_(FusionGroupType &, fusion::InstPtrListType &,
                               fusion::InstPtrListType &)
        {
            return false;
        }
    };
} // namespace olympia
