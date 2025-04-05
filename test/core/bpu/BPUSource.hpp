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
            in_bpu_credits_.registerConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(BPUSource, getCreditsFromBPU_, uint32_t));

            if (params->input_file != "")
            {
                inst_generator_ = olympia::InstGenerator::createGenerator(
                    mavis_facade_, params->input_file, false);
            }
        }

      private:
        const std::string test_type_;
        olympia::MavisType* mavis_facade_ = nullptr;
        std::unique_ptr<olympia::InstGenerator> inst_generator_;

        uint32_t bpu_credits_ = 0;

        std::list<olympia::BranchPredictor::PredictionRequest> pred_request_buffer_;

        const uint32_t pred_req_buffer_capacity = 8;

        void getCreditsFromBPU_(const uint32_t & credits)
        {
            bpu_credits_ += credits;
            ILOG("Received " << credits << " credits from BPU");

            //ev_send_pred_req_.schedule(sparta::Clock::Cycle(0));
            sendPredictionRequest_();
        }

        void sendPredictionRequest_()
        {
            if (bpu_credits_ > 0)
            {
                ILOG("Sending PredictionRequest from Fetch to BPU");
                olympia::BranchPredictor::PredictionRequest pred_request;
                pred_request.instType_ = 1;
                pred_request.PC_ = 5;
                out_bpu_prediction_request_.send(pred_request);
                bpu_credits_--;
            }
        }

        ////////////////////////////////////////////////////////////////////////////////
        // Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataOutPort<olympia::BranchPredictor::PredictionRequest>
            out_bpu_prediction_request_{&unit_port_set_, "out_bpu_prediction_request"};

        sparta::DataInPort<uint32_t> in_bpu_credits_{&unit_port_set_, "in_bpu_credits", 0};

        //sparta::Event<> ev_send_pred_req_{&unit_event_set_, "ev_send_pred_req_",
        //                                  CREATE_SPARTA_HANDLER(BPUSource, sendPredictionRequest_)};
    };

    using SrcFactory = sparta::ResourceFactory<BPUSource, BPUSource::BPUSourceParameters>;
} // namespace bpu_test