// <ExecutePipePipe.cpp> -*- C++ -*-

#include "ExecutePipe.hpp"
#include "CoreUtils.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace olympia
{
    const char ExecutePipe::name[] = "exe_pipe";

    ExecutePipe::ExecutePipe(sparta::TreeNode* node, const ExecutePipeParameterSet* p) :
        sparta::Unit(node),
        ignore_inst_execute_time_(p->ignore_inst_execute_time),
        execute_time_(p->execute_time),
        enable_random_misprediction_(p->enable_random_misprediction && p->contains_branch_unit),
        issue_queue_name_(p->iq_name),
        collected_inst_(node, node->getName())
    {
        p->enable_random_misprediction.ignore();
        p->contains_branch_unit.ignore();
        in_reorder_flush_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(
            ExecutePipe, flushInst_, FlushManager::FlushingCriteria));
        // Startup handler for sending initiatl credits
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(ExecutePipe, setupExecutePipe_));

        ILOG("ExecutePipe construct: #" << node->getGroupIdx());
    }

    void ExecutePipe::setupExecutePipe_()
    {
        // Setup scoreboard view upon register file
        std::vector<core_types::RegFile> reg_files = {core_types::RF_INTEGER, core_types::RF_FLOAT};
        // if we ever move to multicore, we only want to have resources look for
        // scoreboard in their cpu if we're running a test where we only have
        // top.rename or top.issue_queue, then we can just use the root

        // internal parameter, which scoreboard views it has
        auto cpu_node = getContainer()->findAncestorByName("core.*");
        if (cpu_node == nullptr)
        {
            cpu_node = getContainer()->getRoot();
        }
        for (const auto rf : reg_files)
        {
            // alu0, alu1 name is based on exe names, point to issue_queue name instead
            scoreboard_views_[rf].reset(
                new sparta::ScoreboardView(issue_queue_name_, core_types::regfile_names[rf],
                                           cpu_node)); // name needs to come from issue_queue
        }
    }

    // change to insertInst
    void ExecutePipe::insertInst(const InstPtr & ex_inst)
    {
        sparta_assert_context(
            unit_busy_ == false,
            "ExecutePipe is receiving a new instruction when it's already busy!!");
        ex_inst->setStatus(Inst::Status::SCHEDULED);
        const uint32_t exe_time =
            ignore_inst_execute_time_ ? execute_time_ : ex_inst->getExecuteTime();
        collected_inst_.collectWithDuration(ex_inst, exe_time);
        ILOG("Executing: " << ex_inst << " for " << exe_time + getClock()->currentCycle());
        sparta_assert(exe_time != 0);

        unit_busy_ = true;
        execute_inst_.preparePayload(ex_inst)->schedule(exe_time);
    }

    // Called by the scheduler, scheduled by complete_inst_.
    void ExecutePipe::executeInst_(const InstPtr & ex_inst)
    {
        ILOG("Executed inst: " << ex_inst);
        auto reg_file = ex_inst->getRenameData().getDestination().rf;
        if (reg_file != core_types::RegFile::RF_INVALID)
        {
            const auto & dest_bits = ex_inst->getDestRegisterBitMask(reg_file);
            scoreboard_views_[reg_file]->setReady(dest_bits);
        }

        if (enable_random_misprediction_)
        {
            if (ex_inst->isBranch() && (std::rand() % 20) == 0)
            {
                ILOG("Randomly injecting a mispredicted branch: " << ex_inst);
                ex_inst->setMispredicted();
            }
        }

        // We're not busy anymore
        unit_busy_ = false;

        // Count the instruction as completely executed
        ++total_insts_executed_;

        // Schedule completion
        complete_inst_.preparePayload(ex_inst)->schedule(1);
    }

    // Called by the scheduler, scheduled by complete_inst_.
    void ExecutePipe::completeInst_(const InstPtr & ex_inst)
    {
        ex_inst->setStatus(Inst::Status::COMPLETED);
        complete_event_.collect(*ex_inst);
        ILOG("Completing inst: " << ex_inst);
        out_execute_pipe_.send(1);
    }

    void ExecutePipe::flushInst_(const FlushManager::FlushingCriteria & criteria)
    {
        ILOG("Got flush for criteria: " << criteria);
        // Cancel outstanding instructions awaiting completion and
        // instructions on their way to issue
        auto flush = [criteria](const InstPtr & inst) -> bool
        { return criteria.includedInFlush(inst); };
        issue_inst_.cancel();
        complete_inst_.cancelIf(flush);
        execute_inst_.cancelIf(flush);
        if (execute_inst_.getNumOutstandingEvents() == 0)
        {
            unit_busy_ = false;
            collected_inst_.closeRecord();
        }
    }

} // namespace olympia
