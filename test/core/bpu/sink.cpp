#include "sink.hpp"

namespace olympia
{
    sink::sink(sparta::TreeNode* n, const sinkParameterSet* p) :
        sparta::Unit(n),
    {
        // register handle all ports
        in_bpu_predOutput_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(receivePrediction_, PredictionOutput));
    }

    void sink::sendCreditsToBPU_() {
        out_bpu_sinkCredits_.send(1);
    }

    void sink::receivePrediction_(const PredictionOutput & pred_output) {
        pred_output_buffer_.push_back(pred_output);
    }
}