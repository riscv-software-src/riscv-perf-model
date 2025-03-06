#pragma once

#include "core/InstGenerator.hpp"
#include "core/decode/MavisUnit.hpp"
#include "core/InstGroup.hpp"
#include "mavis/ExtractorDirectInfo.h"
#include "core/MemoryAccessInfo.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/LogUtils.hpp"

#include "OlympiaAllocators.hpp"

#include "../../../core/fetch/BPU.hpp"

#include <list>

namespace bpu_test
{
    class BPUSource : public sparta::Unit
    {
      public:
        static constexpr char name[] = "bpu_source_unit";

        class BPUSourceParameters : public sparta::ParameterSet
        {
          public:
            explicit BPUSourceParameters(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

            PARAMETER(std::string, test_type, "single", "Test mode to run: single or multiple")

            PARAMETER(std::string, input_file, "", "Input file: STF or JSON")
        };

        BPUSource(sparta::TreeNode* n, const BPUSourceParameters* params) :
            sparta::Unit(n),
            test_type_(params->test_type),
            mavis_facade_(olympia::getMavis(n))
        {
            sparta_assert(mavis_facade_ != nullptr, "Could not find the Mavis Unit");
            in_bpu_predictionRequest_credits_.registerConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(BPUSource, receivePredictionRequestCredits_,
                                                uint32_t));

            if (params->input_file != "")
            {
                inst_generator_ = olympia::InstGenerator::createGenerator(
                    mavis_facade_, params->input_file, false);
            }
        }

        void initiate()
        {
            // olympia::BranchPredictor::PredictionRequest pred;
            generatedPredictedRequest_.emplace_back();
        }

      private:
        const std::string test_type_;
        olympia::MavisType* mavis_facade_ = nullptr;
        std::unique_ptr<olympia::InstGenerator> inst_generator_;

        void receivePredictionRequestCredits_(const uint32_t & credits)
        {
            ILOG("Received prediction request credits from BPU");
            predictionRequestCredits_ += credits;

            if (predictionRequestCredits_ > 0)
            {
                ev_gen_insts_.schedule();
            }
        }

        void sendPredictionRequest_()
        {
            if (predictionRequestCredits_ > 0)
            {
                auto output = generatedPredictedRequest_.front();
                generatedPredictedRequest_.pop_front();
                out_bpu_predictionRequest_.send(output);
                predictionRequestCredits_--;
            }
        }

        uint32_t predictionRequestCredits_ = 0;
        std::list<olympia::BranchPredictor::PredictionRequest> generatedPredictedRequest_;

        ////////////////////////////////////////////////////////////////////////////////
        // Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataOutPort<olympia::BranchPredictor::PredictionRequest> out_bpu_predictionRequest_{
            &unit_port_set_, "out_bpu_predictionRequest"};

        sparta::DataInPort<uint32_t> in_bpu_predictionRequest_credits_{
            &unit_port_set_, "in_bpu_predictionRequest_credits", 0};

        sparta::SingleCycleUniqueEvent<> ev_gen_insts_{
            &unit_event_set_, "gen_inst", CREATE_SPARTA_HANDLER(BPUSource, sendPredictionRequest_)};
    };

    using SrcFactory = sparta::ResourceFactory<BPUSource, BPUSource::BPUSourceParameters>;
} // namespace bpu_test