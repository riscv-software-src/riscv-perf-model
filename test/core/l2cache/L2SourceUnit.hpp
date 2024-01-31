#pragma once


#include "core/MemoryAccessInfo.hpp"
#include "core/InstGenerator.hpp"
#include "core/MavisUnit.hpp"
#include "mavis/ExtractorDirectInfo.h"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/LogUtils.hpp"

#include <string>

namespace l2cache_test
{
    // ////////////////////////////////////////////////////////////////////////////////
    // "Source" unit, just Sources instructions sent to it.  Sends credits
    // back as directed by params/execution mode
    class L2SourceUnit : public sparta::Unit
    {
    public:
        static constexpr char name[] = "L2SourceUnit";

        class L2SourceUnitParameters : public sparta::ParameterSet
        {
        public:
            explicit L2SourceUnitParameters(sparta::TreeNode *n) :
                sparta::ParameterSet(n)
            { }
            PARAMETER(bool, unit_enable, true, "Is this unit enabled?")
            PARAMETER(std::string, input_file, "", "Input file: STF or JSON")
            PARAMETER(sparta::Clock::Cycle, delay_btwn_insts, 50, "Clock delay between instruction/requests to L2Cache")
        };

        L2SourceUnit(sparta::TreeNode * n, const L2SourceUnitParameters * params)
            : sparta::Unit(n),
              mavis_facade_(olympia::getMavis(n)),
              delay_btwn_insts_(params->delay_btwn_insts),
              unit_enable_(params->unit_enable) {

            sparta_assert(mavis_facade_ != nullptr, "Could not find the Mavis Unit");

            in_source_resp_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(L2SourceUnit, ReceiveInst_, olympia::MemoryAccessInfoPtr));
            in_source_ack_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(L2SourceUnit, ReceiveAck_, uint32_t));

            if(params->input_file != "") {
                inst_generator_ = olympia::InstGenerator::createGenerator(mavis_facade_, params->input_file, false);
            }

            if (unit_enable_ == true)
                sparta::StartupEvent(n, CREATE_SPARTA_HANDLER(L2SourceUnit, injectInsts_));
        }

        ~L2SourceUnit() { }

        void onStartingTeardown_() override {
            sparta_assert(unit_enable_ == true && pending_reqs_ == 0, "pending_reqs remaining in the L2SourceUnit");
            sparta_assert(unit_enable_ == true && pending_acks_ == 0, "pending_acks remaining in the L2SourceUnit");
        }

    private:

        void injectInsts_() {
            olympia::InstPtr dinst;

            if(inst_generator_) {

                while (!inst_generator_->isDone()) {

                    dinst = inst_generator_->getNextInst(getClock());
                    dinst->setUniqueID(unique_id_++);

                    olympia::MemoryAccessInfoPtr mem_info_ptr(new olympia::MemoryAccessInfo(dinst));

                    req_inst_queue_.emplace_back(mem_info_ptr);
                    ev_req_inst_.schedule(schedule_time_);

                    schedule_time_ += delay_btwn_insts_;
                }
            }
        }

        void req_inst_() {

            ILOG("Instruction: '" << req_inst_queue_.front()->getInstPtr() << "' Requested");

            pending_reqs_++;
            pending_acks_++;

            out_source_req_.send(req_inst_queue_.front());
            req_inst_queue_.erase(req_inst_queue_.begin());
        }

        void ReceiveInst_(const olympia::MemoryAccessInfoPtr & mem_info_ptr) {
            pending_reqs_--;
            ILOG("Instruction: '" << mem_info_ptr->getInstPtr() << "' Received");
        }

        void ReceiveAck_(const uint32_t & ack) {
            pending_acks_--;
            ILOG("Ack: '" << ack << "' Received");
        }

        sparta::DataInPort<olympia::MemoryAccessInfoPtr>  in_source_resp_ {&unit_port_set_, "in_source_resp",
                sparta::SchedulingPhase::Tick, 1};
        sparta::DataInPort<uint32_t>                      in_source_ack_  {&unit_port_set_, "in_source_ack"};

        sparta::DataOutPort<olympia::MemoryAccessInfoPtr> out_source_req_ {&unit_port_set_, "out_source_req"};

        uint32_t pending_acks_ = 1;
        uint32_t pending_reqs_ = 0;

        uint32_t unique_id_ = 0;

        olympia::MavisType     * mavis_facade_ = nullptr;
        std::unique_ptr<olympia::InstGenerator> inst_generator_;

        // Event to issue request to L2Cache
        sparta::UniqueEvent<> ev_req_inst_
            {&unit_event_set_, "req_inst", CREATE_SPARTA_HANDLER(L2SourceUnit, req_inst_)};

        std::vector<olympia::MemoryAccessInfoPtr> req_inst_queue_;
        sparta::Clock::Cycle schedule_time_ = 0;
        sparta::Clock::Cycle delay_btwn_insts_ = 0;
        bool unit_enable_;
    };
}
