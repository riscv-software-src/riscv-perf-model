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
            sparta::StartupEvent(n, CREATE_SPARTA_HANDLER(BPUSink, sendPredictionOutputCredits_));
        }

      private:
        uint32_t predictionOutputCredits_ = 1;

        void sendPredictionOutputCredits_()
        {
            ILOG("send prediction output credits to bpu");
            out_bpu_predictionOutput_credits_.send(predictionOutputCredits_);
        }

        void recievePredictionOutput_(const olympia::BranchPredictor::PredictionOutput & predOutput)
        {
            ILOG("recieve prediction output from bpu");
            predictionOutputResult_.push_back(predOutput);
            ev_return_credits_.schedule(1);
        }

        std::list<olympia::BranchPredictor::PredictionOutput> predictionOutputResult_;

        ////////////////////////////////////////////////////////////////////////////////
        // Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataInPort<olympia::BranchPredictor::PredictionOutput> in_bpu_predictionOutput_{
            &unit_port_set_, "in_bpu_predictionOutput", 0};

        sparta::DataOutPort<uint32_t> out_bpu_predictionOutput_credits_{
            &unit_port_set_, "out_bpu_predictionOutput_credits"};

        sparta::UniqueEvent<> ev_return_credits_{
            &unit_event_set_, "return_credits",
            CREATE_SPARTA_HANDLER(BPUSink, sendPredictionOutputCredits_)};
    };

    using SinkFactory = sparta::ResourceFactory<BPUSink, BPUSink::BPUSinkParameters>;
} // namespace bpu_test