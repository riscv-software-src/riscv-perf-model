#include "Bpu_unit.hpp"
#include "sparta/utils/LogUtils.hpp"

namespace olympia
{
    Bpu_unit::Bpu_unit(sparta::TreeNode* node, const Bpu_unitParameterSet* p) :
        sparta::Unit(node),
        ghr_size_(p->ghr_size),
        pht_size_(p->pht_size),
        pht_ctr_bits_(p->pht_ctr_bits)
    {
        in_bpu_predInput_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(receivePredictionInput_, PredictionInput));
        in_bpu_sinkCredits_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(receiveSinkCredits_, uint32_t));
        in_dut_flush_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA());

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER());
    }
    // function definitions below
    void Bpu_unit::sendInitialCredits_() {
        out_src_credits_.send(1);
    }
    void Bpu_unit::receivePredictionInput_(const PredictionInput & pred_input) {
        predInput_buffer_.push_back(pred_input);
    }
    uint8_t Bpu_unit::predictBranch(int idx) {

    }

    PredictionOutput genOutput(uint8_t pred) {
        PredictionOutput temp;
        temp.predDirection = pred;
    }

    void Bpu_unit::receiveSinkCredits_(const uint32_t & credits) {
        sink_credits_ += credits;
        
        // put some mechanism to send prediction output to sink below this
    }
    void sendPrediction_() {
        
    }
}