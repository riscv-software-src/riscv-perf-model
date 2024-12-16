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
#include <map>

namespace olympia
{
    class Bpu_unit : public sparta::Unit
    {
        public:
            class Bpu_unitParameterSet : public sparta::ParameterSet
            {
                public:
                    Bpu_unitParameterSet(sparta::TreeNode *n) : sparta::ParameterSet(n)
                    {}

                    PARAMETER(uint32_t, ghr_size, 1000, "Size of GHR")
                    PARAMETER(uint32_t, pht_size, 1024, "Size of PHT")
                    PARAMETER(uint32_t, pht_ctr_bits, 2, "Counter bits of PHT")
            };
            Bpu_unit(sparta::TreeNode* node, const Bpu_unitParameterSet* p);

            static constexpr char name[] = "Bpu_unit";

        private:
            // Input port to BPU from source
            sparta::DataInPort<PredictionInput> in_bpu_predInput_{&unit_port_set_, "in_bpu_predInput", 1};
            // Output port of BPU to send credit to source
            sparta::DataOutPort<uint32_t> out_src_credits_{&unit_port_set_, "out_src_credits"};

            // output port to sink from BPU
            sparta::DataOutPort<PredictionOutput> out_sink_predOutput_{&unit_port_set_, "out_sink_predOutput"};
            // input port of BPU to receive credit from sink
            sparta::DataInPort<uint32_t> in_bpu_sinkCredits_{&unit_port_set_, "in_bpu_sinkCredits", sparta::SchedulingPhase::Tick, 0};

            // for flush
            sparta::DataInPort<FlushManager::FlushingCriteria> in_dut_flush_{&unit_port_set_, "in_dut_flush", sparta::SchedulingPhase::Flush, 1};

            // TODO: put events here

            // send credits from BPU to source
            void sendInitialCredits_();
            // receive prediction input from source
            void receivePredictionInput_(const PredictionInput & pred_input);
            // mechanism to make prediction
            uint8_t predictBranch(int idx);
            // generate prediction output
            PredictionOutput genOutput(uint8_t pred);
            // receive credits from sink
            void receiveSinkCredits_(const uint32_t & credits);
            // send prediction output to sink
            void sendPrediction_();

            uint32_t sink_credits_ = 0;
            uint32_t ghr_size_;
            uint32_t pht_size_;
            uint32_t pht_ctr_bits_;
            std::vector<PredictionInput> predInput_buffer_;

            std::map<uint32_t idx, uint8_t ctr> pattern_history_table_;


    }
}