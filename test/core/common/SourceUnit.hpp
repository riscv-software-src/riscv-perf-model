
#pragma once

#include "core/InstGenerator.hpp"
#include "core/MavisUnit.hpp"
#include "core/InstGroup.hpp"

#include "mavis/ExtractorDirectInfo.h"

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/ports/DataPort.hpp"

#include <string>

namespace core_test
{

    ////////////////////////////////////////////////////////////////////////////////
    // Source unit, just sends a barrage of instructions into the DUT
    // based on parameters/extensions
    class SourceUnit : public sparta::Unit
    {
    public:
        static constexpr char name[] = "source_unit";

        class SourceUnitParameters : public sparta::ParameterSet
        {
        public:
            explicit SourceUnitParameters(sparta::TreeNode *n) :
                sparta::ParameterSet(n)
            {
            }
            PARAMETER(std::string, test_type, "single", "Test mode to run: single or multiple")
        };

        SourceUnit(sparta::TreeNode * n,
                   const SourceUnitParameters * params) :
            sparta::Unit(n),
            test_type_(params->test_type),
            mavis_facade_(olympia::getMavis(n))
        {
            in_credits_.
                registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(SourceUnit, inCredits<0>, uint32_t));
        }

        void setInstGenerator(olympia::InstGenerator * inst_gen) {
            inst_generator_ = inst_gen;
        }

        void injectInsts();

        template<uint32_t pipeline_id>
        void inCredits(const uint32_t & credits) {
            info_logger_ << "Got credits from dut: " << credits;
            dut_credits_ = credits;

            if(dut_credits_ > 0) {
                ev_gen_insts_.schedule();
            }
        }

    private:

        const std::string test_type_;
        uint32_t          inst_cnt_  = 0;
        uint64_t          unique_id_ = 0;

        sparta::DataOutPort<olympia::InstGroupPtr> out_instgrp_write_{&unit_port_set_, "out_instgrp_write"};
        sparta::DataInPort<uint32_t>               in_credits_       {&unit_port_set_, "in_credits", 0};

        uint32_t dut_credits_{0};

        olympia::MavisType     * mavis_facade_ = nullptr;
        olympia::InstGenerator * inst_generator_ = nullptr;

        sparta::SingleCycleUniqueEvent<> ev_gen_insts_{&unit_event_set_, "gen_inst",
                                                       CREATE_SPARTA_HANDLER(SourceUnit, injectInsts)};
    };

    using SourceUnitFactory = sparta::ResourceFactory<SourceUnit, SourceUnit::SourceUnitParameters>;
}
