
#include "fetch/BPU.hpp"

namespace olympia
{
    namespace BranchPredictor
    {
        const char* BPU::name = "BPU";

        BPU::BPU(sparta::TreeNode* node, const BPUParameterSet* p) : sparta::Unit(node)
        {
            in_fetch_predictionRequest_.registerConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(BPU, recievePredictionRequest_, PredictionRequest));

            in_fetch_predictionOutput_credits_.registerConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(BPU, receivePredictionOutputCredits_, uint32_t));
        }

        PredictionOutput BPU::getPrediction(const PredictionRequest & pred)
        {
            PredictionOutput output;
            output.predDirection_ = true;
            output.predPC_ = 0x0220;

            return output;
        }

        void BPU::updatePredictor(const UpdateInput & update) {}

        BPU::~BPU() {}

        void BPU::sendPredictionRequestCredits_(uint32_t credits)
        {
            ILOG("Send prediction request credits to Fetch");
            out_fetch_predictionRequest_credits_.send(credits);
        }

        void BPU::sendInitialPredictionRequestCredits_()
        {
            ILOG("Sending initial prediction request credits to Fetch");
            sendPredictionRequestCredits_(1);
        }

        void BPU::recievePredictionRequest_(const PredictionRequest & predReq)
        {
            ILOG("Received prediction request from Fetch");
            predictionRequestBuffer_.push_back(predReq);
        }

        // void BPU::recievePredictionUpdate_()
        //{}

        void BPU::receivePredictionOutputCredits_(const uint32_t & credits)
        {
            ILOG("Recieve prediction output credits from Fetch");
            predictionOutputCredits_ += credits;
        }

        void BPU::makePrediction_()
        {
            PredictionOutput output;
            output.predDirection_ = true;
            output.predPC_ = 100000;
            generatedPredictionOutputBuffer_.push_back(output);
        }

        void BPU::sendPrediction_()
        {
            if (predictionOutputCredits_ > 0)
            {
                ILOG("Sending prediction output to fetch");
                auto predOutput = generatedPredictionOutputBuffer_.front();
                generatedPredictionOutputBuffer_.pop_front();
                out_fetch_predictionOutput_.send(predOutput);
                predictionOutputCredits_--;
            }
        }

    } // namespace BranchPredictor
} // namespace olympia
