#pragma once 

#include "FlushManager.hpp"
#include "InstGroup.hpp"
#include "BPTypes.hpp"

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

#include <vector>

namespace olympia
{
    class sink : public sparta::Unit
    {
        public:
            class sinkParamterSet : public sparta::ParameterSet
            {
                public:
                    sinkParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n)
                    {}
            };
            sink(sparta::TreeNode* n, const sinkParameterSet* p);

            void sendCreditsToBPU_();

            void receivePrediction_(const PredictionOutput & pred_output);

        private:
            // input port to receive prediction output from bpu
            sparta::DataInPort<PredictionOutput> in_bpu_predOutput{&unit_port_set_, "in_bpu_predOutput"}; // yet to add cycle delay

            // port to send credits to bpu
            sparta::DataOutPort<uint32_t> out_bpu_sinkCredits_{&unit_port_set, "out_bpu_sinkCredits"};

            std::vector<PredictionOutput> pred_output_buffer_;


    }
}