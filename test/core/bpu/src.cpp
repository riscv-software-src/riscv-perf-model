#include "src.hpp"

namespace olympia
{
    src::src(sparta::TreeNode* n, const srcParameterSet* p) :
        sparta::Unit(n)
    {
        // register handle all ports
        in_bpu_credits_.regsiterConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(receieveCreditsFromBPU_, uint32_t));
    }



    void src::readInstruction_() {
        olympia::InstGroupPtr inst_groups(new olympia::InstGroup);
        olympia::Inst::PtrType dinst;

        if (inst_generator_)
        {
            if (dinst = inst_generator_->getNextInst(getClock()), nullptr != dinst)
            {
                dinst->setUniqueID(unique_id_++);
                inst_groups->emplace_back(dinst);
                --dut_credits_;
            }
        }
        // Send inst group to the BPU
        if (!inst_groups->empty())
        {
            sendPredictionRequest_(inst_group);
        }

    }

    void src::sendPredictionRequest_(olympia::InstGroupPtr & inst_group) {
            // extract relevant data from inst_groups, generate predictionRequest
            // then send to BPU through out_bpu_predReq_
    }

    void src::receieveCreditsFromBPU_(const uint32_t & credits) {
        bpu_credits_ += credits;
    }
}