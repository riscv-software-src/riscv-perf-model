#include "FTQ.hpp"

namespace olympia
{
    const char* FTQ::name = "ftq";

    FTQ::FTQ(sparta::TreeNode* node, const FTQParameterSet* p) :
        sparta::Unit(node),
        ftq_capacity_(p->ftq_capacity)
    {
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(FTQ, sendInitialCreditsToBPU_));

        in_bpu_first_prediction_output_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(
            FTQ, getFirstPrediction_, BranchPredictor::PredictionOutput));

        in_bpu_second_prediction_output_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(
            FTQ, getSecondPrediction_, BranchPredictor::PredictionOutput));

        in_fetch_credits_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(FTQ, getFetchCredits_, uint32_t));

        /**in_rob_signal_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(FTQ, getROBSignal_, uint32_t));
            **/
    }

    FTQ::~FTQ() {}

    void FTQ::sendInitialCreditsToBPU_() { sendCreditsToBPU_(5); }

    void FTQ::sendCreditsToBPU_(const uint32_t & credits)
    {
        ILOG("Send " << credits << " credits to BPU");
        out_bpu_credits_.send(credits);
    }

    void FTQ::getFirstPrediction_(const BranchPredictor::PredictionOutput & prediction)
    {
        if (fetch_target_queue_.size() < ftq_capacity_)
        {
            ILOG("FTQ receives first PredictionOutput from BPU");
            fetch_target_queue_.push_back(prediction);

            if(fetch_target_queue_.size() == 1) {
                ftq_it = fetch_target_queue_.begin();
            }
            sendPrediction_();
        }
    }

    void FTQ::getSecondPrediction_(const BranchPredictor::PredictionOutput & prediction)
    {
        // check if it matches the prediction made by first tier of bpu
        ILOG("FTQ receives second PredictionOutput from BPU");
        handleMismatch(prediction);
    }

    #define TAKEN 0
    #define NOT_TAKEN 1

    void FTQ::handleMismatch(const BranchPredictor::PredictionOutput & tage_prediction)
    {
        ILOG("Checking mismatch between BasePredictor and TAGE_SC_L");

        std::deque<BranchPredictor::PredictionOutput> :: iterator it = fetch_target_queue_.begin();
        while(it != fetch_target_queue_.end()) {
            if(it->instrPC == tage_prediction.instrPC) {
                ILOG("BasePredictor prediction direction: " << it->predDirection_);
                ILOG("TAGE_SC_L prediction direction: " << tage_prediction.predDirection_);
                if(it->predDirection_ != tage_prediction.predDirection_) {
                    ILOG("Prediction mismatch between tage and base predictor");

                    /***
                     * TODO:
                     * We need to update predPC_ in case there is an update of output
                     * How to do it in case a not taken prediction is updated to taken?
                     */
                    if(it->predDirection_ == TAKEN && tage_prediction.predDirection_ == NOT_TAKEN) {
                        it->predPC_ = it->instrPC + 4; 
                    }
                    else if(it->predDirection_ == NOT_TAKEN && tage_prediction.predDirection_ == TAKEN) {
                        // ??
                    }

                    it->predDirection_ = tage_prediction.predDirection_;

                    ftq_it = it;

                    sendPrediction_();
                    break;
                }
            }
        }
    }

    void FTQ::getFetchCredits_(const uint32_t & credits)
    {
        ILOG("FTQ: Received " << credits << " credits from Fetch")
        fetch_credits_ += credits;

        sendPrediction_();
    }

    void FTQ::sendPrediction_()
    {
        if (fetch_credits_ > 0)
        {
            // send prediction to Fetch
            if (fetch_target_queue_.size() > 0)
            {
                fetch_credits_--;
                ILOG("Send prediction from FTQ to Fetch");
                auto output = *ftq_it;
                ftq_it++;
                
                out_fetch_prediction_output_.send(output);
            }
        }
    }

    void FTQ::firstMispredictionFlush_() {}

    void FTQ::getROBSignal_(const uint32_t & signal) {}

    void FTQ::deallocateEntry_() {}
} // namespace olympia