#pragma once

#include "sparta/utils/LogUtils.hpp"
#include "core/MemoryAccessInfo.hpp"
#include "core/InstGenerator.hpp"
#include "sparta/utils/LogUtils.hpp"

namespace dcache_test
{
    class SourceUnit : public sparta::Unit
    {
      public:
        static constexpr char name[] = "SourceUnit";

        class SourceUnitParameters : public sparta::ParameterSet
        {
          public:
            explicit SourceUnitParameters(sparta::TreeNode* n) : sparta::ParameterSet(n) {}
            PARAMETER(std::string, input_file, "", "Input file: STF or JSON")
            PARAMETER(sparta::Clock::Cycle, delay_btwn_insts, 1,
                      "Clock delay between instruction/requests to DCache")
        };

        SourceUnit(sparta::TreeNode* n, const SourceUnitParameters* params) :
            sparta::Unit(n),
            mavis_facade_(olympia::getMavis(n)),
            delay_btwn_insts_(params->delay_btwn_insts)
        {

            sparta_assert(mavis_facade_ != nullptr, "Could not find the Mavis Unit");

            in_source_resp_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(
                SourceUnit, ReceiveInst_, olympia::MemoryAccessInfoPtr));
            in_source_ack_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(
                SourceUnit, ReceiveAck_, olympia::MemoryAccessInfoPtr));

            if (params->input_file != "")
            {
                inst_generator_ = olympia::InstGenerator::createGenerator(
                    info_logger_, mavis_facade_, params->input_file, false);
            }

            sparta::StartupEvent(n, CREATE_SPARTA_HANDLER(SourceUnit, sendInitialInst_));
        }

        ~SourceUnit() {}

        void onStartingTeardown_() override {}

      private:
        void sendInitialInst_() { injectInsts_(); }

        void injectInsts_()
        {
            olympia::InstPtr dinst;

            if (inst_generator_)
            {

                while (!inst_generator_->isDone())
                {

                    dinst = inst_generator_->getNextInst(getClock());
                    dinst->setUniqueID(unique_id_++);

                    olympia::MemoryAccessInfoPtr mem_info_ptr(new olympia::MemoryAccessInfo(dinst));

                    req_inst_queue_.emplace_back(mem_info_ptr);
                    ev_req_inst_.schedule(schedule_time_);

                    schedule_time_ += delay_btwn_insts_;
                }
            }
        }

        void req_inst_()
        {

            ILOG("Instruction: '" << req_inst_queue_.front()->getInstPtr() << "' Requested");

            pending_reqs_++;
            pending_acks_++;

            out_source_req_.send(req_inst_queue_.front());
            req_inst_queue_.erase(req_inst_queue_.begin());
        }

        void ReceiveInst_(const olympia::MemoryAccessInfoPtr & mem_info_ptr)
        {
            pending_reqs_--;
            ILOG("Instruction: '" << mem_info_ptr->getInstPtr() << "' Received");
        }

        void ReceiveAck_(const olympia::MemoryAccessInfoPtr & mem_info_ptr)
        {
            pending_acks_--;
            ILOG("Ack: '" << mem_info_ptr << "' Received");
        }

        sparta::DataInPort<olympia::MemoryAccessInfoPtr> in_source_resp_{
            &unit_port_set_, "in_source_resp", sparta::SchedulingPhase::Tick, 1};
        sparta::DataInPort<olympia::MemoryAccessInfoPtr> in_source_ack_{&unit_port_set_,
                                                                        "in_source_ack"};

        sparta::DataOutPort<olympia::MemoryAccessInfoPtr> out_source_req_{&unit_port_set_,
                                                                          "out_source_req"};

        uint32_t pending_acks_ = 1;
        uint32_t pending_reqs_ = 0;

        uint32_t unique_id_ = 0;

        olympia::MavisType* mavis_facade_ = nullptr;
        std::unique_ptr<olympia::InstGenerator> inst_generator_;

        sparta::UniqueEvent<> ev_req_inst_{&unit_event_set_, "req_inst",
                                           CREATE_SPARTA_HANDLER(SourceUnit, req_inst_)};

        std::vector<olympia::MemoryAccessInfoPtr> req_inst_queue_;
        sparta::Clock::Cycle schedule_time_ = 0;
        sparta::Clock::Cycle delay_btwn_insts_ = 0;
    };
} // namespace dcache_test