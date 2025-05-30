#pragma once

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/utils/LogUtils.hpp"

#include "BranchPredIF.hpp"
#include "BasePredictor.hpp"
#include "TAGE_SC_L.hpp"

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

            // PC of instruction for which PredictionOutput is generated 
            // this can be used as an index to find mismatch and update prediction 
            // between BasePredictor and TAGE_SC_L.
            uint64_t instrPC;
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
            UpdateInput() {}

            // UpdateInput(uint64_t instrPC, bool correctedDirection, uint64_t correctedTargetPC) {}

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

                // TODO: choose default values properly
                // currently values were chosen randomly
                PARAMETER(uint32_t, ghr_size, 1024, "Number of branch history bits stored in GHR");
                PARAMETER(uint32_t, ghr_hash_bits, 4,
                          "Number of bits from GHR used for hashing with PC, to index PHT")
                PARAMETER(uint32_t, pht_size, 1024, "Number of entries stored in PHT")
                PARAMETER(uint32_t, ctr_bits, 8,
                          "Number of bits used by counter in PHT to make prediction")
                PARAMETER(uint32_t, btb_size, 512, "Maximum possible number of entries in BTB")
                PARAMETER(uint32_t, ras_size, 128, "Maximum possible number of entries in RAS")
                PARAMETER(bool, ras_enable_overwrite, true,
                          "New entries on maximum capacity overwrite")
                PARAMETER(uint32_t, tage_bim_table_size, 1024, "Size of TAGE bimodal table")
                PARAMETER(uint32_t, tage_bim_ctr_bits, 8,
                          "Number of bits used by TAGE bimodal table to make prediction")
                PARAMETER(uint32_t, tage_tagged_table_num, 6,
                          "Number of tagged components in TAGE predictor")
                PARAMETER(uint32_t, logical_table_num, 8, "Number of logical table in SC")
                PARAMETER(uint32_t, loop_pred_table_size, 64,
                          "Maximum possible entries in loop predictor table")
                PARAMETER(uint32_t, loop_pred_table_way, 4, "Way size of loop predictor table")
            };

            BPU(sparta::TreeNode* node, const BPUParameterSet* p);

            ~BPU();

            PredictionOutput getPrediction(const PredictionRequest &);
            void updatePredictor(const UpdateInput &);

            //! \brief Name of this resource. Required by sparta::UnitFactory
            static const char* name;

          private:
            void getPredictionRequest_(const PredictionRequest &);
            void makePrediction_();
            void getCreditsFromFTQ_(const uint32_t &);
            void sendFirstPrediction_();
            void sendSecondPrediction_();
            void getUpdateInput_(const UpdateInput &);
            void updateBPU_(const UpdateInput &);
            void sendCreditsToFetch_(const uint32_t &);
            void sendIntitialCreditsToFetch_();
            void updateGHRTaken_();
            void updateGHRNotTaken_();

            const uint32_t ghr_size_;
            const uint32_t ghr_hash_bits_;
            const uint32_t pht_size_;
            const uint32_t ctr_bits_;
            const uint32_t btb_size_;
            const uint32_t ras_size_;
            const bool ras_enable_overwrite_;
            const uint32_t tage_bim_table_size_;
            const uint32_t tage_bim_ctr_bits_;
            const uint32_t tage_tagged_table_num_;
            const uint32_t logical_table_num_;
            const uint32_t loop_pred_table_size_;
            const uint32_t loop_pred_table_way_;

            std::list<PredictionRequest> predictionRequestBuffer_;
            std::list<PredictionOutput> generatedPredictionOutputBuffer_;
            const uint32_t pred_req_buffer_capacity_ = 10;

            uint32_t ftq_credits_ = 0;
            // UpdateInput internal_update_input_;

            BasePredictor base_predictor_;
            Tage tage_predictor_;

            ///////////////////////////////////////////////////////////////////////////////
            // Ports
            ///////////////////////////////////////////////////////////////////////////////

            // Internal DataInPort from Fetch unit for PredictionRequest
            sparta::DataInPort<PredictionRequest> in_fetch_prediction_request_{
                &unit_port_set_, "in_fetch_prediction_request", sparta::SchedulingPhase::Tick, 0};

            // Internal DataInPort from FTQ unit for credits to indicate
            // availabilty of slots for sending PredictionOutput
            sparta::DataInPort<uint32_t> in_ftq_credits_{&unit_port_set_, "in_ftq_credits",
                                                         sparta::SchedulingPhase::Tick, 0};
            // Internal DataInPort from FTQ for UpdateInput
            sparta::DataInPort<UpdateInput> in_ftq_update_input_{
                &unit_port_set_, "in_ftq_update_input", sparta::SchedulingPhase::Tick, 0};

            // DataOutPort to Fetch unit to send credits to indicate availability
            // of slots to receive PredictionRequest
            sparta::DataOutPort<uint32_t> out_fetch_credits_{&unit_port_set_, "out_fetch_credits"};

            // DataOutPort to FTQ unit to send prediction made by BasePredictor
            sparta::DataOutPort<PredictionOutput> out_ftq_first_prediction_output_{
                &unit_port_set_, "out_ftq_first_prediction_output"};

            // DataOutPort to FTQ unit to send prediction made by TAGE_SC_L
            sparta::DataOutPort<PredictionOutput> out_ftq_second_prediction_output_{
                &unit_port_set_, "out_ftq_second_prediction_output"};

            //////////////////////////////////////////////////////////////////////////////
            // Events
            //////////////////////////////////////////////////////////////////////////////
            /***sparta::PayloadEvent<PredictionOutput> ev_sendPrediction_{
                &unit_event_set_, "ev_sendPrediction",
                CREATE_SPARTA_HANDLER_WITH_DATA(BPU, sendPrediction_, PredictionOutput)};
                ***/

            sparta::UniqueEvent<> ev_send_first_prediction_{
                &unit_event_set_, "ev_send_first_prediction_",
                CREATE_SPARTA_HANDLER(BPU, sendFirstPrediction_)};

            sparta::UniqueEvent<> ev_send_second_prediction_{
                &unit_event_set_, "ev_send_second_prediction_",
                CREATE_SPARTA_HANDLER(BPU, sendSecondPrediction_)};

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
