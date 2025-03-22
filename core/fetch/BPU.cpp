
#include "fetch/BPU.hpp"

namespace olympia
{
    namespace BranchPredictor
    {
        const char* BPU::name = "bpu";

        BPU::BPU(sparta::TreeNode* node, const BPUParameterSet* p) :
            sparta::Unit(node),
            ghr_size_(p->ghr_size),
            ghr_hash_bits_(p->ghr_hash_bits),
            pht_size_(p->pht_size),
            ctr_bits_(p->ctr_bits),
            btb_size_(p->btb_size),
            ras_size_(p->ras_size),
            ras_enable_overwrite_(p->ras_enable_overwrite),
            tage_bim_table_size_(p->tage_bim_table_size),
            tage_bim_ctr_bits_(p->tage_bim_ctr_bits),
            tage_tagged_table_num_(p->tage_tagged_table_num),
            logical_table_num_(p->logical_table_num),
            loop_pred_table_size_(p->loop_pred_table_size),
            loop_pred_table_way_(p->loop_pred_table_way)
            //base_predictor_(pht_size_, ctr_bits_, btb_size_, ras_size_)
        {
            sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(BPU, sendIntitialCreditsToFetch_));

            in_fetch_prediction_request_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(BPU, getPredictionRequest_, PredictionRequest));

            in_ftq_credits_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(BPU, getCreditsFromFTQ_, uint32_t));

            in_ftq_update_input_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(BPU, getUpdateInput_, UpdateInput));
        }

        BPU::~BPU() {}

        PredictionOutput BPU::getPrediction(const PredictionRequest &) {
            PredictionOutput output;
            output.predDirection_ = true;
            output.predPC_ = 5;
            return output;
        }

        void BPU::updatePredictor(const UpdateInput &) {

        }

        void BPU::getPredictionRequest_(const PredictionRequest & request) {
            predictionRequestBuffer_.push_back(request);
            ILOG("BPU: received PredictionRequest from Fetch");

            makePrediction_();
        }

        void BPU::makePrediction_() {
            ILOG("making prediction");
            std::cout << "making prediction\n";
            if(predictionRequestBuffer_.size() > 0) {
                //auto input = predictionRequestBuffer_.front();
                predictionRequestBuffer_.pop_front();

                // call base predictor on input
                PredictionOutput output;

                output.predDirection_ = true;
                output.predPC_ = 100;
                generatedPredictionOutputBuffer_.push_back(output);
                
                // call tage_sc_l on input
            }
        }

        void BPU::getCreditsFromFTQ_(const uint32_t & credits) {
            predictionOutputCredits_ += credits;
            ILOG("BPU: received " << credits << " credits from FTQ");

            sendFirstPrediction_();
        }

        
        void BPU::sendFirstPrediction_() {
            ILOG("SendFirstPrediction starting");
            // take first PredictionRequest from buffer
            if(predictionOutputCredits_ > 0) 
            {
                auto firstPrediction = generatedPredictionOutputBuffer_.front();
                generatedPredictionOutputBuffer_.pop_front();
                ILOG("BPU: Sending first PredictionOutput to FTQ");
                out_ftq_first_prediction_output_.send(firstPrediction);
                predictionOutputCredits_--;
            }
        }

        /**
        void BPU::sendSecondPrediction_() {
            // send prediction made by TAGE_SC_L
        }
***/
        void BPU::getUpdateInput_(const UpdateInput & input) {
            //internal_update_input_ = input;

            ILOG("BPU: received UpdateInput from FTQ");

           // updateBPU_(internal_update_input_);
        }
        /**

        void BPU::updateBPU_(const UpdateInput & input) {

            // Update internal state of BasePredictor according to UpdateInput received
            //base_predictor_.update(input);

            // Update internal state of TAGE_SC_L according to UpdateInput received
            // TODO 
            // tage_sc_l.update(input);
        }
            **/
        

        void BPU::sendCreditsToFetch_(const uint32_t & credits) {
            ILOG("BPU: Send " << credits << " credits to Fetch");
            out_fetch_credits_.send(credits);
        }

        void BPU::sendIntitialCreditsToFetch_() {
            sendCreditsToFetch_(pred_req_buffer_capacity_);
        }

        void BPU::updateGHRTaken_() {

        }

        void BPU::updateGHRNotTaken_() {

        }
    } // namespace BranchPredictor
} // namespace olympia
