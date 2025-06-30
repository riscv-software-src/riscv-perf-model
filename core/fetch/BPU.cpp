
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
            loop_pred_table_way_(p->loop_pred_table_way),
            base_predictor_(pht_size_, ctr_bits_, btb_size_, ras_size_, ras_enable_overwrite_),
            tage_predictor_(tage_bim_table_size_, tage_bim_ctr_bits_, /*5,*/ 2, 3, 10, 2, 2, 1024,
                            tage_tagged_table_num_, 10)
        {
            sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(BPU, sendIntitialCreditsToFetch_));

            in_fetch_prediction_request_.registerConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(BPU, getPredictionRequest_, PredictionRequest));

            in_ftq_credits_.registerConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(BPU, getCreditsFromFTQ_, uint32_t));

            in_ftq_update_input_.registerConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(BPU, getUpdateInput_, UpdateInput));
        }

        BPU::~BPU() {}

        PredictionOutput BPU::getPrediction(const PredictionRequest &)
        {
            PredictionOutput output;
            output.predDirection_ = true;
            output.predPC_ = 5;
            return output;
        }

        void BPU::updatePredictor(const UpdateInput &) {}

        void BPU::getPredictionRequest_(const PredictionRequest & request)
        {
            predictionRequestBuffer_.push_back(request);
            ILOG("BPU received PredictionRequest from Fetch");
        }

        /***void BPU::makePrediction_() {
            ILOG("making prediction");
            std::cout << "making prediction\n";
            if(predictionRequestBuffer_.size() > ev_make_second_prediction_0) {
                //auto input = predictionRequestBuffer_.front();
                predictionRequestBuffer_.pop_front();

                // call base predictor on input
                PredictionOutput output;

                output.predDirection_ = true;
                output.predPC_ = 100;
                generatedPredictionOutputBuffer_.push_back(output);

                sendFirstPrediction_();

                // call tage_sc_l on input
            }
        }**/

        void BPU::getCreditsFromFTQ_(const uint32_t & credits)
        {
            ftq_credits_ += credits;
            ILOG("BPU received " << credits << " credits from FTQ");
            ev_send_first_prediction_.schedule(1);
            ev_send_second_prediction_.schedule(4);
        }

        void BPU::sendFirstPrediction_()
        {
            // take first PredictionRequest from buffer
            if (ftq_credits_ > 0 && predictionRequestBuffer_.size() > 0)
            {
                PredictionOutput output;

                PredictionRequest in = predictionRequestBuffer_.front();

                ILOG("Getting direction from base predictor");
                bool dir = base_predictor_.getDirection(in.PC_, in.instType_);
                ILOG("Getting target from base predictor");
                uint64_t target = base_predictor_.getTarget(in.PC_, in.instType_);

                output.instrPC = in.PC_;
                output.predPC_ = target;
                output.predDirection_ = dir;

                generatedPredictionOutputBuffer_.push_back(output);

                auto firstPrediction = generatedPredictionOutputBuffer_.front();
                generatedPredictionOutputBuffer_.pop_front();
                ILOG("Sending first PredictionOutput from BPU to FTQ");
                out_ftq_first_prediction_output_.send(firstPrediction);
                ftq_credits_--;
            }
        }

        void BPU::sendSecondPrediction_()
        {
            // send prediction made by TAGE_SC_L
            PredictionOutput output;

            PredictionRequest in = predictionRequestBuffer_.front();

            ILOG("Getting direction prediction from TAGE");
            output.instrPC = in.PC_;
            output.predDirection_ = tage_predictor_.predict(in.PC_);
            // TAGE only predicts whether branch will be taken or not, so predPC_ value will be ignored
            output.predPC_ = 0;
            ILOG("Sending second PredictionOutput from BPU to FTQ");
            out_ftq_second_prediction_output_.send(output);
        }

        void BPU::getUpdateInput_(const UpdateInput & input)
        {
            // internal_update_input_ = input;

            ILOG("BPU received UpdateInput from FTQ");

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

        void BPU::sendCreditsToFetch_(const uint32_t & credits)
        {
            ILOG("Send " << credits << " credits from BPU to Fetch");
            out_fetch_credits_.send(credits);
        }

        void BPU::sendIntitialCreditsToFetch_() { sendCreditsToFetch_(pred_req_buffer_capacity_); }

        void BPU::updateGHRTaken_() {}

        void BPU::updateGHRNotTaken_() {}
    } // namespace BranchPredictor
} // namespace olympia
