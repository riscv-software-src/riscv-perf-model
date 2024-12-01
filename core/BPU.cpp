#include "BPU.hpp"
#include "BPTypes.hpp"

#include <iostream>

namespace olympia
{
    BPU::name = "bpu"; 
    BPU::BPU(sparta::TreeNode* node, BPUParameterSet* p) :
        sparta::Unit(node),
        ghr_size_(p->ghr_size)

        {
            in_fetch_prediction_credits_.createConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(BPU, receivePredictionCredits_, uint32_t));

            in_fetch_prediction_req_.createConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(BPU, receivePredictionInput_, PredictionInput));

        }
    void BPU::receivePredictionCredits_(uint32_t credit) {
        std::cout << credit << std::endl;
    }

    void BPU::recievePredictionInput(PredictionInput) {
        std::cout << "hello" << std::endl;
    }

    void BPU::updateGHRTaken() {
        ghr_ = ghr_ << 1;
        ghr_ = ghr_ | 0b1;
    }

    void BPU::updateGHRNotTaken() {
        ghr_ = ghr_ << 1;
        ghr_ = ghr_ & 0b0; 
    }
}


