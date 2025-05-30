#pragma once

#include "core/MemoryAccessInfo.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/LogUtils.hpp"

#include "../../../core/fetch/BPU.hpp"

#include <list>

namespace bpu_test
{
    class BPUSink : public sparta::Unit
    {
      public:
        static constexpr char name[] = "bpu_sink_unit";

        class BPUSinkParameters : public sparta::ParameterSet
        {
          public:
            BPUSinkParameters(sparta::TreeNode* n) : sparta::ParameterSet(n) {}
        };

        BPUSink(sparta::TreeNode* n, const BPUSinkParameters* params) : sparta::Unit(n)
        {
            sparta::StartupEvent(n, CREATE_SPARTA_HANDLER(BPUSink, sendInitialCreditsToFTQ_));

            in_ftq_prediction_output_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(
                BPUSink, getPredictionOutput_, olympia::BranchPredictor::PredictionOutput));
        }

      private:
        std::list<olympia::BranchPredictor::PredictionOutput> pred_output_buffer_;
        uint32_t ftq_credits_ = 0;

        void sendCreditsToFTQ_(const uint32_t & credits)
        {
            ILOG("Send " << credits << " credits from Fetch to FTQ");
            out_ftq_credits_.send(credits);
        }

        void sendInitialCreditsToFTQ_() { sendCreditsToFTQ_(5); }

        void getPredictionOutput_(const olympia::BranchPredictor::PredictionOutput & output)
        {
            ILOG("Fetch recieves prediction output from FTQ");
            pred_output_buffer_.push_back(output);
        }

        ////////////////////////////////////////////////////////////////////////////////
        // Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataInPort<olympia::BranchPredictor::PredictionOutput> in_ftq_prediction_output_{
            &unit_port_set_, "in_ftq_prediction_output", 0};

        sparta::DataOutPort<uint32_t> out_ftq_credits_{&unit_port_set_, "out_ftq_credits"};

        /**sparta::UniqueEvent<> ev_return_credits_{
            &unit_event_set_, "return_credits",
            CREATE_SPARTA_HANDLER(BPUSink, sendPredictionOutputCredits_)};
            ***/
    };

    using SinkFactory = sparta::ResourceFactory<BPUSink, BPUSink::BPUSinkParameters>;
} // namespace bpu_test