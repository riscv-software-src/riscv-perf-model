#include "FTQ.hpp"

namespace olympia
{
    const char* FTQ::name = "ftq";

    FTQ::FTQ(sparta::TreeNode *node, const FTQParameterSet *p) :
        sparta::Unit(node),
        ftq_capacity_(p->ftq_capacity)
        {
            in_bpu_first_prediction_output_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(FTQ, getFirstPrediction_, BranchPredictor::PredictionOutput));

            in_bpu_second_prediction_output_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(FTQ, getSecondPrediction_, BranchPredictor::PredictionOutput));

            in_fetch_credits_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(FTQ, getFetchCredits_, uint32_t));

            /**in_rob_signal_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(FTQ, getROBSignal_, uint32_t));
                **/
            sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(FTQ, sendInitialCreditsToBPU_));
        }
    
    FTQ::~FTQ() {}

    void FTQ::sendInitialCreditsToBPU_() 
    {
        sendCreditsToBPU_(5);
    }

    void FTQ::sendCreditsToBPU_(const uint32_t & credits) 
    {
        ILOG("FTQ: Sending " << credits << " credits to BPU");
        out_bpu_credits_.send(credits);
    }

    void FTQ::getFirstPrediction_(const BranchPredictor::PredictionOutput & prediction)
    {
        if(fetch_target_queue_.size() < ftq_capacity_) {
            ILOG("FTQ: Received first PredictionOutput from BPU");
            fetch_target_queue_.push(prediction);

            sendPrediction_();
        }
    }

    void FTQ::getSecondPrediction_(const BranchPredictor::PredictionOutput & prediction)
    {
        // check if it matches the prediction made by first tier of bpu
        
        
    }

    void FTQ::getFetchCredits_(const uint32_t & credits)
    {
        ILOG("FTQ: Received " << credits << " credits from Fetch")
        fetchCredits_ += credits;

        sendPrediction_();
    }

    void FTQ::sendPrediction_()
    {
        ILOG("In sendPrediction function")
        if(fetchCredits_ > 0) {
            ILOG("fetchCredits > 0");
            // send prediction to Fetch
            if(fetch_target_queue_.size() > 0) {
                fetchCredits_--;
                ILOG("FTQ: Send prediction to Fetch");
                auto output = fetch_target_queue_.front();
                fetch_target_queue_.pop();
                out_fetch_prediction_output_.send(output);
            }
        }
    }

    void FTQ::firstMispredictionFlush_()
    {

    }

    void FTQ::getROBSignal_(const uint32_t & signal)
    {

    }

    void FTQ::deallocateEntry_()
    {

    }
}