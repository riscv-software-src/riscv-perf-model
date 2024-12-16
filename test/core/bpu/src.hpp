#pragma once

#include "FlushManager.hpp"
#include "InstGroup.hpp"
#include "BPTypes.hpp"

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

namespace olympia
{
    class src : public sparta::Unit
    {
        public:
            class srcParamterSet : public sparta::ParamterSet
            {
                public:
                    srcParameterSet(sparta::TreeNode *n) : sparta::ParameterSet(n)
                    {}
            };
            src(sparta::TreeNode* n, const srcParameterSet* p);

        private:
            // port from source to bpu to send prediction input
            sparta::DataOutPort<PredictionInput> out_bpu_predReq_{&unit_port_set_, "out_bpu_predReq"};

            // input port in bpu to receive credits from bpu
            sparta::DataInPort<uint32_t> in_bpu_credits_{&unit_port_set_, "in_bpu_credits", 0};

            // todo: sparta event comes here

            uint32_t bpu_credits_ = 0;

            void receieveCreditsFromBPU_(const uint32_t & credits);
            void readInstruction_();
            void sendPredictionRequest_(olympia::InstGroupPtr & inst_group);
    };

    using srcFactory = sparta::ResourceFactory<src, src::srcParameterSet>;
}