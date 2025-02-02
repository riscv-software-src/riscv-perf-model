#pragma once

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/utils/LogUtils.hpp"

#include "BranchPredIF.hpp"

#include <list>

namespace olympia
{
    namespace BranchPredictor
    {
        class PredictionRequest
        {
          public:
            PredictionRequest() {}

            uint64_t PC_;
            uint8_t instType_;

            friend std::ostream & operator<<(std::ostream & os, const PredictionRequest &)
            {
                return os;
            }
        };

        class PredictionOutput
        {
          public:
            PredictionOutput() {}

            bool predDirection_;
            uint64_t predPC_;

            friend std::ostream & operator<<(std::ostream & os, const PredictionOutput &)
            {
                return os;
            }
        };

        class UpdateInput
        {
          public:
            UpdateInput(uint64_t instrPC, bool correctedDirection, uint64_t correctedTargetPC) {}

            uint64_t instrPC_;
            bool correctedDirection_;
            uint64_t correctedTargetPC_;

            friend std::ostream & operator<<(std::ostream & os, const UpdateInput &) { return os; }
        };

        class BPU :
            public BranchPredictorIF<PredictionOutput, UpdateInput, PredictionRequest>,
            public sparta::Unit
        {
          public:
            class BPUParameterSet : public sparta::ParameterSet
            {
              public:
                BPUParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

                PARAMETER(uint32_t, ghr_size, 1024, "Number of history bits in GHR");
            };

            BPU(sparta::TreeNode* node, const BPUParameterSet* p);

            PredictionOutput getPrediction(const PredictionRequest &);
            void updatePredictor(const UpdateInput &);

            ~BPU();

            // DefaultPrediction getPrediction(const PredictionRequest &);
            // void updatePredictor(const UpdateInput &);

            //! \brief Name of this resource. Required by sparta::UnitFactory
            static const char* name;

          private:
            void sendPredictionRequestCredits_(uint32_t credits);
            void sendInitialPredictionRequestCredits_();
            void recievePredictionRequest_(const PredictionRequest & predReq);
            // void recievePredictionUpdate_();
            void receivePredictionOutputCredits_(const uint32_t & credits);
            void makePrediction_();
            void sendPrediction_();

            std::list<PredictionRequest> predictionRequestBuffer_;
            std::list<PredictionOutput> generatedPredictionOutputBuffer_;
            uint32_t predictionRequestCredits_ = 0;
            uint32_t predictionOutputCredits_ = 0;

            ///////////////////////////////////////////////////////////////////////////////
            // Ports
            ///////////////////////////////////////////////////////////////////////////////

            // Internal DataInPort from fetch unit for prediction request
            sparta::DataInPort<PredictionRequest> in_fetch_predictionRequest_{
                &unit_port_set_, "in_fetch_predictionRequest", sparta::SchedulingPhase::Tick, 0};

            // Internal DataInPort from fetch unit for credits to indicate
            // availabilty of slots for sending prediction output
            sparta::DataInPort<uint32_t> in_fetch_predictionOutput_credits_{
                &unit_port_set_, "in_fetch_predictionOutput_credits", sparta::SchedulingPhase::Tick,
                0};

            // TODO

            // DataOutPort to fetch unit to send credits to indicate availability
            // of slots to receive prediction request
            sparta::DataOutPort<uint32_t> out_fetch_predictionRequest_credits_{
                &unit_port_set_, "out_fetch_predictionRequest_credits"};

            // DataOutPort to fetch unit to send prediction output
            sparta::DataOutPort<PredictionOutput> out_fetch_predictionOutput_{
                &unit_port_set_, "out_fetch_predictionOutput"};

            //////////////////////////////////////////////////////////////////////////////
            // Events
            //////////////////////////////////////////////////////////////////////////////
            /***sparta::PayloadEvent<PredictionOutput> ev_sendPrediction_{
                &unit_event_set_, "ev_sendPrediction",
                CREATE_SPARTA_HANDLER_WITH_DATA(BPU, sendPrediction_, PredictionOutput)};
                ***/

            //////////////////////////////////////////////////////////////////////////////
            // Counters
            //////////////////////////////////////////////////////////////////////////////
            sparta::Counter pred_req_num_{getStatisticSet(), "pred_req_num",
                                          "Number of prediction requests",
                                          sparta::Counter::COUNT_NORMAL};
            sparta::Counter mispred_num_{getStatisticSet(), "mispred_num",
                                         "Number of mis-predictions",
                                         sparta::Counter::COUNT_NORMAL};
            sparta::StatisticDef mispred_ratio_{getStatisticSet(), "mispred_ratio",
                                                "Percenatge of mis-prediction", getStatisticSet(),
                                                "mispred_num/pred_req_num"};
            sparta::Counter branch_req_num_{getStatisticSet(), "branch_req_num",
                                            "Number of branch requests",
                                            sparta::Counter::COUNT_NORMAL};
            sparta::Counter call_req_num_{getStatisticSet(), "call_req_num",
                                          "Number of call requests", sparta::Counter::COUNT_NORMAL};
            sparta::Counter return_req_num_{getStatisticSet(), "return_req_num",
                                            "Number of return requests",
                                            sparta::Counter::COUNT_NORMAL};
            sparta::Counter pht_req_num_{getStatisticSet(), "pht_req_num",
                                         "Number of requests made to PHT",
                                         sparta::Counter::COUNT_NORMAL};
            sparta::Counter pht_hit_num_{getStatisticSet(), "pht_hit_num", "Number of hits on PHT",
                                         sparta::Counter::COUNT_NORMAL};
            sparta::Counter pht_miss_num_{getStatisticSet(), "pht_miss_num",
                                          "Number of misses on PHT", sparta::Counter::COUNT_NORMAL};
            sparta::StatisticDef pht_mispred_ratio_{getStatisticSet(), "pht_mispred_ratio",
                                                    "Percentage of PHT mis-prediction",
                                                    getStatisticSet(), "pht_miss_num/pht_req_num"};
            sparta::Counter btb_req_num_{getStatisticSet(), "btb_req_num",
                                         "Number of requests to BTB",
                                         sparta::Counter::COUNT_NORMAL};
            sparta::Counter btb_hit_num_{getStatisticSet(), "btb_hit_num", "NUmber of BTB hits",
                                         sparta::Counter::COUNT_NORMAL};
            sparta::Counter btb_miss_num_{getStatisticSet(), "btb_miss_num", "Number of BTB misses",
                                          sparta::Counter::COUNT_NORMAL};
            sparta::StatisticDef btb_hit_rate_{getStatisticSet(), "btb_hit_rate",
                                               "Rate of BTB hits", getStatisticSet(),
                                               "btb_hit_num/btb_req_num"};
            sparta::StatisticDef btb_miss_rate_{getStatisticSet(), "btb_miss_rate",
                                                "Rate of BTB misses", getStatisticSet(),
                                                "btb_miss_num/btb_req_num"};
            sparta::Counter ras_high_mark_{getStatisticSet(), "ras_high_mark", "RAS high mark",
                                           sparta::Counter::COUNT_NORMAL};
            sparta::Counter ras_low_mark_{getStatisticSet(), "ras_low_mark", "RAS low mark",
                                          sparta::Counter::COUNT_NORMAL};
        };
    } // namespace BranchPredictor
} // namespace olympia
