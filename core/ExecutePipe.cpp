// <ExecutePipePipe.cpp> -*- C++ -*-

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "ExecutePipe.hpp"
#include "CoreUtils.hpp"

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
        appendIssueQueue_(ex_inst);
        sparta_assert(ex_inst->getStatus() == Inst::Status::DISPATCHED, "Bad instruction status: " << ex_inst);
        scheduleInst_(ex_inst);
    }

    void ExecutePipe::scheduleInst_(const InstPtr & ex_inst)
    {
        // FIXME: Now every source operand should be ready
        const auto & src_bits = ex_inst->getSrcRegisterBitMask(reg_file_);
        if(scoreboard_views_[reg_file_]->isSet(src_bits))
        {
            // Insert at the end if we are doing in order issue or if the scheduler is empty
            ILOG("Sending to issue queue " << ex_inst);
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
            scoreboard_views_[reg_file_]->
                registerReadyCallback(src_bits, ex_inst->getUniqueID(),
                                      [this, ex_inst](const sparta::Scoreboard::RegisterBitMask&)
                                      {
                                          this->scheduleInst_(ex_inst);
                                      });
            ILOG("Instruction NOT ready: " << ex_inst << " Bits needed:" << sparta::printBitSet(src_bits));
        }
    }

    void ExecutePipe::issueInst_()
    {
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
        // Pop the insturction from the ready queue and send a credit back to dispatch
        popIssueQueue_(ex_inst_ptr);
        ready_queue_.pop_front();
        out_scheduler_credits_.send(1, 0);
    }

    // Called by the scheduler, scheduled by complete_inst_.
    void ExecutePipe::completeInst_(const InstPtr & ex_inst)
    {
        ex_inst->setStatus(Inst::Status::COMPLETED);
        ILOG("Completing inst: " << ex_inst);

        // set scoreboard
        if(SPARTA_EXPECT_FALSE(ex_inst->isTransfer()))
        {
            if(ex_inst->getPipe() == InstArchInfo::TargetPipe::I2F)
            {
                // Integer source -> FP dest -- need to mark the appropriate destination SB
                sparta_assert(reg_file_ == core_types::RegFile::RF_INTEGER,
                              "Got an I2F instruction in an ExecutionPipe that does not source the integer RF: " << ex_inst);
                const auto & dest_bits = ex_inst->getDestRegisterBitMask(core_types::RegFile::RF_FLOAT);
                scoreboard_views_[core_types::RegFile::RF_FLOAT]->setReady(dest_bits);
            }
            else {
                // FP source -> Integer dest -- need to mark the appropriate destination SB
                sparta_assert(ex_inst->getPipe() == InstArchInfo::TargetPipe::F2I,
                              "Instruction is marked transfer type, but I2F nor F2I: " << ex_inst);
                sparta_assert(reg_file_ == core_types::RegFile::RF_FLOAT,
                              "Got an F2I instruction in an ExecutionPipe that does not source the Float RF: " << ex_inst);
                const auto & dest_bits = ex_inst->getDestRegisterBitMask(core_types::RegFile::RF_INTEGER);
                scoreboard_views_[core_types::RegFile::RF_INTEGER]->setReady(dest_bits);
            }
        }
        else {
            const auto & dest_bits = ex_inst->getDestRegisterBitMask(reg_file_);
            scoreboard_views_[reg_file_]->setReady(dest_bits);
        }

        // Deal with mispredicted branches
        if (ex_inst->isBranch() && ex_inst->isBranchMispredict())
        {
            ILOG("mispredicted branch " << ex_inst <<
                  " was actually " << (ex_inst->isTakenBranch() ? "taken" : "not-taken"));
            FlushManager::FlushingCriteria criteria(FlushManager::FlushEvent::MISPREDICTION, ex_inst);
            out_execute_flush_.send(criteria);
        }

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

        // Flush instructions in the issue queue
        uint32_t credits_to_send = 0;

        auto iter = issue_queue_.begin();
        while (iter != issue_queue_.end())
        {
            auto delete_iter = iter++;

            auto inst_ptr = *delete_iter;

            // Remove flushed instruction from issue queue and clear scoreboard callbacks
            if (criteria.flush(inst_ptr))
            {
                issue_queue_.erase(delete_iter);

                scoreboard_views_[reg_file_]->clearCallbacks(inst_ptr->getUniqueID());

                ++credits_to_send;

                ILOG("Flush Instruction ID: " << inst_ptr->getUniqueID());
            }
        }

        if(credits_to_send) {
            out_scheduler_credits_.send(credits_to_send, 0);

            ILOG("Flush " << credits_to_send << " instructions in issue queue!");
        }

        // Flush instructions in the ready queue
        auto flush_criteria = [criteria](const InstPtr & inst) -> bool {
            return criteria.flush(inst);
        };
        ready_queue_.erase(std::remove_if(ready_queue_.begin(),
                                          ready_queue_.end(),
                                          flush_criteria),
                           ready_queue_.end());

        // Cancel outstanding instructions awaiting completion and
        // instructions on their way to issue
        complete_inst_.cancelIf(flush_criteria);
        issue_inst_.cancel();

        if(complete_inst_.getNumOutstandingEvents() == 0) {
            unit_busy_ = false;
            collected_inst_.closeRecord();
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Regular Function/Subroutine Call
    ////////////////////////////////////////////////////////////////////////////////

    // Append instruction into issue queue
    void ExecutePipe::appendIssueQueue_(const InstPtr & inst_ptr)
    {
        sparta_assert(issue_queue_.size() < scheduler_size_,
                        "Appending issue queue causes overflows!");

        issue_queue_.emplace_back(inst_ptr);
    }

    // Pop completed instruction out of issue queue
    void ExecutePipe::popIssueQueue_(const InstPtr & inst_ptr)
    {
        // Look for the instruction to be completed, and remove it from issue queue
        for (auto iter = issue_queue_.begin(); iter != issue_queue_.end(); iter++) {
            if (*iter == inst_ptr) {
                issue_queue_.erase(iter);
                return;
            }
        }
        sparta_assert(false, "Attempt to complete instruction no longer exiting in issue queue!");
    }


}
