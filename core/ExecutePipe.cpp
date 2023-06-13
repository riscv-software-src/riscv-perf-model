// <ExecutePipePipe.cpp> -*- C++ -*-

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "ExecutePipe.hpp"

namespace olympia
{
    core_types::RegFile determineRegisterFile(const std::string & target_name)
    {
        if(target_name == "alu" || target_name == "br") {
            return core_types::RF_INTEGER;
        }
        else if(target_name == "fpu") {
            return core_types::RF_FLOAT;
        }
        sparta_assert(false, "Not supported this target: " << target_name);
    }

    const char ExecutePipe::name[] = "exe_pipe";

    ExecutePipe::ExecutePipe(sparta::TreeNode * node,
                             const ExecutePipeParameterSet * p) :
        sparta::Unit(node),
        ignore_inst_execute_time_(p->ignore_inst_execute_time),
        execute_time_(p->execute_time),
        scheduler_size_(p->scheduler_size),
        in_order_issue_(p->in_order_issue),
        reg_file_(determineRegisterFile(node->getGroup())),
        collected_inst_(node, node->getName())
    {
        in_execute_inst_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(ExecutePipe, getInstsFromDispatch_,
                                                                    InstPtr));

        in_reorder_flush_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(ExecutePipe, flushInst_,
                                                                    FlushManager::FlushingCriteria));
        // Startup handler for sending initiatl credits
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(ExecutePipe, setupExecutePipe_));
        // Set up the precedence between issue and complete
        // Complete should come before issue because it schedules issue with a 0 cycle delay
        // issue should always schedule complete with a non-zero delay (which corresponds to the
        // insturction latency)
        complete_inst_ >> issue_inst_;

        ILOG("ExecutePipe construct: #" << node->getGroupIdx());

    }

    void ExecutePipe::setupExecutePipe_()
    {
        // Setup scoreboard view upon register file
        std::vector<core_types::RegFile> reg_files = {core_types::RF_INTEGER, core_types::RF_FLOAT};
        for(const auto rf : reg_files)
        {
            scoreboard_views_[rf].reset(new sparta::ScoreboardView(getContainer()->getName(),
                                                                   core_types::regfile_names[rf],
                                                                   getContainer()));
        }

        // Send initial credits
        out_scheduler_credits_.send(scheduler_size_);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Callbacks
    void ExecutePipe::getInstsFromDispatch_(const InstPtr & ex_inst)
    {
        // FIXME: Now every source operand should be ready
        const auto & src_bits = ex_inst->getSrcRegisterBitMask(reg_file_);
        if(scoreboard_views_[reg_file_]->isSet(src_bits)){
            // Insert at the end if we are doing in order issue or if the scheduler is empty
            ILOG("Sending to issue queue" << ex_inst);
            if (in_order_issue_ == true || ready_queue_.size() == 0) {
                ready_queue_.emplace_back(ex_inst);
            }
            else {
                // Stick the instructions in a random position in the ready queue
                uint64_t issue_pos = uint64_t(std::rand()) % ready_queue_.size();
                if (issue_pos == ready_queue_.size()-1) {
                    ready_queue_.emplace_back(ex_inst);
                }
                else {
                    uint64_t pos = 0;
                    auto iter = ready_queue_.begin();
                    while (iter != ready_queue_.end()) {
                        if (pos == issue_pos) {
                            ready_queue_.insert(iter, ex_inst);
                            break;
                        }
                        ++iter;
                        ++pos;
                    }
                }
            }
            // Schedule issue if the alu is not busy
            if (unit_busy_ == false) {
                issue_inst_.schedule(sparta::Clock::Cycle(0));
            }
        }
        else{
            scoreboard_views_[reg_file_]->registerReadyCallback(src_bits, ex_inst->getUniqueID(),
                                        [this, ex_inst](const sparta::Scoreboard::RegisterBitMask&)
                                        {this->getInstsFromDispatch_(ex_inst);});
            ILOG("Registering Callback: " << ex_inst);
        }
    }

    void ExecutePipe::issueInst_() {
        // Issue a random instruction from the ready queue
        sparta_assert_context(unit_busy_ == false && ready_queue_.size() > 0,
                            "Somehow we're issuing on a busy unit or empty ready_queue");
        // Issue the first instruction
        InstPtr & ex_inst_ptr = ready_queue_.front();
        auto & ex_inst = *ex_inst_ptr;
        ex_inst.setStatus(Inst::Status::SCHEDULED);
        const uint32_t exe_time =
            ignore_inst_execute_time_ ? execute_time_ : ex_inst.getExecuteTime();
        collected_inst_.collectWithDuration(ex_inst, exe_time);
        ILOG("Executing: " << ex_inst << " for "
             << exe_time + getClock()->currentCycle());
        sparta_assert(exe_time != 0);

        ++total_insts_issued_;
        // Mark the instruction complete later...
        complete_inst_.preparePayload(ex_inst_ptr)->schedule(exe_time);
        // Mark the alu as busy
        unit_busy_ = true;
        // Pop the insturction from the scheduler and send a credit back to dispatch
        ready_queue_.pop_front();
        out_scheduler_credits_.send(1, 0);
    }

    // Called by the scheduler, scheduled by complete_inst_.
    void ExecutePipe::completeInst_(const InstPtr & ex_inst) {
        ILOG("Completing inst: " << ex_inst);

        ex_inst->setStatus(Inst::Status::COMPLETED);

        // set scoreboard
        const auto & dest_bits = ex_inst->getDestRegisterBitMask(reg_file_);
        scoreboard_views_[reg_file_]->setReady(dest_bits);

        // We're not busy anymore
        unit_busy_ = false;

        // Count the instruction as completely executed
        ++total_insts_executed_;

        // Schedule issue if we have instructions to issue
        if (ready_queue_.size() > 0) {
            issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    void ExecutePipe::flushInst_(const FlushManager::FlushingCriteria & criteria)
    {
        ILOG("Got flush for criteria: " << criteria);

        // Flush instructions in the ready queue
        ReadyQueue::iterator it = ready_queue_.begin();
        uint32_t credits_to_send = 0;
        while(it != ready_queue_.end()) {
            if((*it)->getUniqueID() >= uint64_t(criteria)) {
                ready_queue_.erase(it++);
                ++credits_to_send;
            }
            else {
                ++it;
            }
        }
        if(credits_to_send) {
            out_scheduler_credits_.send(credits_to_send, 0);
        }

        // Cancel outstanding instructions awaiting completion and
        // instructions on their way to issue
        auto cancel_critera = [criteria](const InstPtr & inst) -> bool {
            if(inst->getUniqueID() >= uint64_t(criteria)) {
                return true;
            }
            return false;
        };
        complete_inst_.cancelIf(cancel_critera);
        issue_inst_.cancel();

        if(complete_inst_.getNumOutstandingEvents() == 0) {
            unit_busy_ = false;
            collected_inst_.closeRecord();
        }
    }

}