#pragma once

#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SignalPort.hpp"

#include "BPTypes.hpp"

#include <climits>
#include <map>
#include <stack>

namespace olympia
{
    class BPU : public sparta::Unit 
    {
        public:
            class BPUParameterSet : public sparta::ParameterSet
            {
                public:
                    BPUParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

                    // Parameters for the Branch Prediction Unit
                    PARAMETER(uint32_t, ghr_size, 1024, "Number of history bits in GHR");
                    
                    
            };
            static const char name[];
            BPU(sparta::TreeNode* n, sparta::ParameterSet* p);

        private:
            void receivePredictionCredits_(uint32_t);
            void recievePredictionInput(PredictionInput);

            // update GHR when last branch is taken
            void updateGHRTaken();
            // update GHR when last branch is not taken
            void updateGHRNotTaken();

            // Number of history bits in GHR
            uint32_t ghr_size_;

            // Global History Register(GHR)
            uint32_t ghr_ = 0;;

            // input ports
            // verify cycle delay required
            sparta::DataInPort<uint32_t> in_fetch_prediction_credits_ {&unit_port_set_,
                                                                    "in_fetch_prediction_credits", 0};

            // verify cycle delay required
            sparta::DataInPort<PredictionInput> in_fetch_prediction_req_ {&unit_port_set_,
                                                                        "in_fetch_prediction_req", 0};

            // input port to receieve update input


            // output ports
            // verify cycle delay required
            sparta::DataOutPort<PredictionOutput> out_fetch_prediction_res_ {&unit_port_set_,
                                                                            "out_fetch_prediction_res", 0};


            // counters
            sparta::Counter pred_req_num_ {getStatisticSet(), "pred_req_num",
                                        "Number of prediction requests made", 
                                        sparta::Counter::COUNT_NORMAL};

            sparta::Counter mispred_num_ {getStatisticSet(), "mispred_num",
                                        "Number of mispredictions",
                                        sparta::Counter::COUNT_NORMAL};

            sparta::StatisticDef mispred_ratio_ {getStatisticSet(), "misprediction ratio",
                                                "misprediction/total_prediction", getStatisticSet(),
                                                "pred_req_num/mispred_num"};
    };
}