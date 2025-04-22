#pragma once

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/utils/DataContainer.hpp"

#include "fetch/BPU.hpp"
#include "ROB.hpp"
#include "FlushManager.hpp"

#include <deque>

namespace olympia
{
    class FTQ : public sparta::Unit
    {
      public:
        class FTQParameterSet : public sparta::ParameterSet
        {
          public:
            FTQParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

            // for now set default to 10, chnage it later
            PARAMETER(uint32_t, ftq_capacity, 10, "Capacity of fetch target queue")
        };

        static const char* name;

        FTQ(sparta::TreeNode* node, const FTQParameterSet* p);

        ~FTQ();

      private:
        const uint32_t ftq_capacity_;

        uint32_t fetch_credits_ = 0;
        std::deque<BranchPredictor::PredictionOutput> fetch_target_queue_;

        // Iterator pointing to next element to send further
        std::deque<BranchPredictor::PredictionOutput> :: iterator ftq_it;

        sparta::DataInPort<BranchPredictor::PredictionOutput> in_bpu_first_prediction_output_{
            &unit_port_set_, "in_bpu_first_prediction_output", 1};
        sparta::DataInPort<BranchPredictor::PredictionOutput> in_bpu_second_prediction_output_{
            &unit_port_set_, "in_bpu_second_prediction_output", 1};
        sparta::DataInPort<uint32_t> in_fetch_credits_{&unit_port_set_, "in_fetch_credits", 1};
        /**sparta::DataOutPort<FlushManager::FlushingCriteria>    out_first_misprediction_flush_
                                                                {&unit_port_set_,
           "out_first_misprediction_flush", 1};***/
        sparta::DataOutPort<BranchPredictor::PredictionOutput> out_fetch_prediction_output_{
            &unit_port_set_, "out_fetch_prediction_output", 1};
        /***sparta::DataInPort<bool> in_rob_signal_ {&unit_port_set_, "in_rob_signal", 1};  ***/
        sparta::DataOutPort<BranchPredictor::UpdateInput> out_bpu_update_input_{
            &unit_port_set_, "out_bpu_update_input", 1};
        sparta::DataOutPort<uint32_t> out_bpu_credits_{&unit_port_set_, "out_bpu_credits", 1};

        void sendInitialCreditsToBPU_();

        void sendCreditsToBPU_(const uint32_t &);

        // receives prediction from BasePredictor and pushes it into FTQ
        void getFirstPrediction_(const BranchPredictor::PredictionOutput &);

        // receives prediction from TAGE_SC_L, checks if there's a mismatch
        // updates ftq appropriately
        void getSecondPrediction_(const BranchPredictor::PredictionOutput &);

        void handleMismatch(const BranchPredictor::PredictionOutput &);

        void getFetchCredits_(const uint32_t &);

        // continuously send instructions to fetch/icache
        void sendPrediction_();

        // flushes instruction if first prediction does not matvh second prediction
        void firstMispredictionFlush_();

        // receives branch resolution signal from ROB at the time of commit
        void getROBSignal_(const uint32_t & signal);

        // deallocate FTQ entry once branch instruction is committed
        void deallocateEntry_();
    };
} // namespace olympia