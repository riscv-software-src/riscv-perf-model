#pragma once

#include "core/inst/InstGenerator.hpp"
#include "core/decode/MavisUnit.hpp"
#include "core/inst/InstGroup.hpp"

#include "mavis/ExtractorDirectInfo.h"

#include "sparta/ports/DataPort.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/LogUtils.hpp"

#include <string>

namespace core_test
{
    // -----------------------------------------------------------------
    // Source unit, sends instructions from either json or stf to the DUT
    // -----------------------------------------------------------------
    class Src : public sparta::Unit
    {
      public:
        static constexpr char name[] = "src";

        class SrcParameters : public sparta::ParameterSet
        {
          public:
            explicit SrcParameters(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

            PARAMETER(std::string, test_type, "single", "Test mode to run: single or multiple")

            PARAMETER(std::string, input_file, "", "Input file: STF or JSON")
        };

        Src(sparta::TreeNode* n, const SrcParameters* params) :
            sparta::Unit(n),
            test_type_(params->test_type),
            mavis_facade_(olympia::getMavis(n))
        {
            sparta_assert(mavis_facade_ != nullptr, "Could not find the Mavis Unit");
            i_credits_.registerConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(Src, inCredits<0>, uint32_t));

            if (params->input_file != "")
            {
                inst_generator_ = olympia::InstGenerator::createGenerator(
                    info_logger_, mavis_facade_, params->input_file, false);
            }
        }

        void injectInsts();

        template <uint32_t pipeline_id> void inCredits(const uint32_t & credits)
        {
            ILOG("Got credits from dut: " << credits);
            dut_credits_ = credits;

            if (dut_credits_ > 0)
            {
                ev_gen_insts_.schedule();
            }
        }

      private:
        const std::string test_type_;
        uint32_t inst_cnt_ = 0;
        uint64_t unique_id_ = 0;

        sparta::DataOutPort<olympia::InstGroupPtr> out_instgrp_write_{&unit_port_set_,
                                                                      "o_instgrp_write"};

        sparta::DataInPort<uint32_t> i_credits_{&unit_port_set_, "i_credits", 0};

        uint32_t dut_credits_{0};

        olympia::MavisType* mavis_facade_ = nullptr;

        std::unique_ptr<olympia::InstGenerator> inst_generator_;

        sparta::SingleCycleUniqueEvent<> ev_gen_insts_{&unit_event_set_, "gen_inst",
                                                       CREATE_SPARTA_HANDLER(Src, injectInsts)};
    };

    using SrcFactory = sparta::ResourceFactory<Src, Src::SrcParameters>;
} // namespace core_test
