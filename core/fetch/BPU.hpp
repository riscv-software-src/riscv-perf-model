#pragma once

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/utils/LogUtils.hpp"

#include "BranchPredIF.hpp"

#include <list>

namespace olympia
{
    namespace BranchPredictor
    {
        class PredictionRequest
        {
          public:
            PredictionRequest() {}

            uint64_t PC_;
            uint8_t instType_;
            friend std::ostream & operator<<(std::ostream &, const PredictionRequest &);
        };

        class PredictionOutput
        {
          public:
            PredictionOutput() {}

            bool predDirection_;
            uint64_t predPC_;
            friend std::ostream & operator<<(std::ostream &, const PredictionOutput &);
        };

        class UpdateInput
        {
          public:
            UpdateInput(uint64_t instrPC, bool correctedDirection, uint64_t correctedTargetPC) {}

            uint64_t instrPC_;
            bool correctedDirection_;
            uint64_t correctedTargetPC_;
            friend std::ostream & operator<<(std::ostream &, const UpdateInput &);
        };

        class BPU :
            public BranchPredictorIF<PredictionOutput, UpdateInput, PredictionRequest>,
            public sparta::Unit
        {
          public:
            class BPUParameterSet : public sparta::ParameterSet
            {
              public:
                BPUParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

                PARAMETER(uint32_t, ghr_size, 1024, "Number of history bits in GHR");
            };

            BPU(sparta::TreeNode* node, const BPUParameterSet* p);

            PredictionOutput getPrediction(const PredictionRequest &);
            void updatePredictor(const UpdateInput &);

            ~BPU();

            // DefaultPrediction getPrediction(const PredictionRequest &);
            // void updatePredictor(const UpdateInput &);

            //! \brief Name of this resource. Required by sparta::UnitFactory
            static const char* name;

          private:
            void sendPredictionRequestCredits_(uint32_t credits);
            void sendInitialPredictionRequestCredits_();
            void recievePredictionRequest_(const PredictionRequest & predReq);
            // void recievePredictionUpdate_();
            void receivePredictionOutputCredits_(const uint32_t & credits);
            void makePrediction_();
            void sendPrediction_();

            std::list<PredictionRequest> predictionRequestBuffer_;
            std::list<PredictionOutput> generatedPredictionOutputBuffer_;
            uint32_t predictionRequestCredits_ = 0;
            uint32_t predictionOutputCredits_ = 0;

            ///////////////////////////////////////////////////////////////////////////////
            // Ports
            ///////////////////////////////////////////////////////////////////////////////

            // Internal DataInPort from fetch unit for prediction request
            sparta::DataInPort<PredictionRequest> in_fetch_predictionRequest_{
                &unit_port_set_, "in_fetch_predictionRequest", sparta::SchedulingPhase::Tick, 0};

            // Internal DataInPort from fetch unit for credits to indicate
            // availabilty of slots for sending prediction output
            sparta::DataInPort<uint32_t> in_fetch_predictionOutput_credits_{
                &unit_port_set_, "in_fetch_predictionOutput_credits", sparta::SchedulingPhase::Tick,
                0};

            // TODO

            // DataOutPort to fetch unit to send credits to indicate availability
            // of slots to receive prediction request
            sparta::DataOutPort<uint32_t> out_fetch_predictionRequest_credits_{
                &unit_port_set_, "out_fetch_predictionRequest_credits"};

            // DataOutPort to fetch unit to send prediction output
            sparta::DataOutPort<PredictionOutput> out_fetch_predictionOutput_{
                &unit_port_set_, "out_fetch_predictionOutput"};

            //////////////////////////////////////////////////////////////////////////////
            // Events
            //////////////////////////////////////////////////////////////////////////////
            /***sparta::PayloadEvent<PredictionOutput> ev_sendPrediction_{
                &unit_event_set_, "ev_sendPrediction",
                CREATE_SPARTA_HANDLER_WITH_DATA(BPU, sendPrediction_, PredictionOutput)};
                ***/
        };
    } // namespace BranchPredictor
} // namespace olympia
