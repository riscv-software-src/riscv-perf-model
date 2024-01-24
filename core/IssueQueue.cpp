// <IssueQueue.cpp> -*- C++ -*-

#include "IssueQueue.hpp"
#include "CoreUtils.hpp"

namespace olympia
{
    const char IssueQueue::name[] = "issue_queue";

    IssueQueue::IssueQueue(sparta::TreeNode* node, const IssueQueueParameterSet* p) :
        sparta::Unit(node),
        reg_file_(olympia::coreutils::determineRegisterFile(
            node->getGroup().substr(node->getGroup().find("_") + 1))),
        ready_queue_(IssueQueueSorter(&iq_sorter)),
        scheduler_size_(p->scheduler_size),
        in_order_issue_(p->in_order_issue)
    {
        in_execute_inst_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(IssueQueue, receiveInstsFromDispatch_, InstPtr));
        in_exe_pipe_done_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(IssueQueue, readyExeUnit_, uint32_t));
        in_reorder_flush_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(
            IssueQueue, flushInst_, FlushManager::FlushingCriteria));
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(IssueQueue, setupIssueQueue_));
        node->getParent()->registerForNotification<bool, IssueQueue, &IssueQueue::onROBTerminate_>(
            this, "rob_stopped_notif_channel", false /* ROB maybe not be constructed yet */);
        iq_sorter.setSorting(p->in_order_issue);
    }

    void IssueQueue::setupIssueQueue_()
    {
        //  if we ever move to multicore, we only want to have resources look for scoreboard in
        //  their cpu if we're running a test where we only have top.rename or top.issue_queue, then
        //  we can just use the root
        auto cpu_node = getContainer()->findAncestorByName("core.*");
        if (cpu_node == nullptr)
        {
            cpu_node = getContainer()->getRoot();
        }
        scoreboard_views_.reset(new sparta::ScoreboardView(
            getContainer()->getName(), core_types::regfile_names[reg_file_], cpu_node));
        out_scheduler_credits_.send(scheduler_size_);
    }

    void IssueQueue::onROBTerminate_(const bool & val) { rob_stopped_simulation_ = val; }

    void IssueQueue::dumpDebugContent_(std::ostream & output) const
    {
        output << "IssueQueue Contents" << std::endl;

        for (const auto & entry : ready_queue_)
        {
            output << '\t' << entry << std::endl;
        }
    }

    void IssueQueue::onStartingTeardown_()
    {
        if ((false == rob_stopped_simulation_) && (false == ready_queue_.empty()))
        {
            dumpDebugContent_(std::cerr);
            sparta_assert(false, "Issue queue has pending instructions");
        }
    }

    void IssueQueue::setExePipe(const std::string & exe_pipe_name, olympia::ExecutePipe* exe_pipe)
    {
        exe_pipes_[exe_pipe_name] = exe_pipe;
    }

    void IssueQueue::setExePipeMapping(const InstArchInfo::TargetPipe tgt_pipe,
                                       olympia::ExecutePipe* exe_pipe)
    {
        if (pipe_exe_pipe_mapping_.find(tgt_pipe) == pipe_exe_pipe_mapping_.end())
        {
            pipe_exe_pipe_mapping_[tgt_pipe] = std::vector<olympia::ExecutePipe*>();
        }
        auto pipe_vector = pipe_exe_pipe_mapping_.find(tgt_pipe);
        pipe_vector->second.emplace_back(exe_pipe);
        pipes_.push_back(exe_pipe);
    }

    void IssueQueue::receiveInstsFromDispatch_(const InstPtr & ex_inst)
    {
        sparta_assert(ex_inst->getStatus() == Inst::Status::DISPATCHED,
                      "Bad instruction status: " << ex_inst);
        appendIssueQueue_(ex_inst);
        handleOperandIssueCheck_(ex_inst);
    }

    void IssueQueue::handleOperandIssueCheck_(const InstPtr & ex_inst)
    {
        // FIXME: Now every source operand should be ready
        const auto & src_bits = ex_inst->getSrcRegisterBitMask(reg_file_);
        if (scoreboard_views_->isSet(src_bits))
        {
            // Insert at the end if we are doing in order issue or if the scheduler is
            // empty
            ILOG("Sending to issue queue " << ex_inst);
            // will insert based on if in_order_issue_ is set
            // if it is, will be first in first out, if not it'll be by age, so by UniqueID (UID)
            ready_queue_.insert(ex_inst);
            ev_issue_ready_inst_.schedule(sparta::Clock::Cycle(0));
        }
        else
        {
            scoreboard_views_->registerReadyCallback(
                src_bits, ex_inst->getUniqueID(),
                [this, ex_inst](const sparta::Scoreboard::RegisterBitMask &)
                { this->handleOperandIssueCheck_(ex_inst); });
            ILOG("Instruction NOT ready: " << ex_inst
                                           << " Bits needed:" << sparta::printBitSet(src_bits));
        }
    }

    void IssueQueue::readyExeUnit_(const uint32_t & readyExe)
    {
        /*
        TODO:
        have a map/mask to see which execution units are ready
        signal port
        */
        if (!ready_queue_.empty())
        {
            ev_issue_ready_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    void IssueQueue::sendReadyInsts_()
    {
        sparta_assert(ready_queue_.size() <= scheduler_size_,
                      "ready queue greater than issue queue size: " << scheduler_size_);
        auto inst_itr = ready_queue_.begin();
        while (inst_itr != ready_queue_.end())
        {
            auto inst = *inst_itr;
            const auto & valid_exe_pipe = pipe_exe_pipe_mapping_[inst->getPipe()];
            bool sent = false;
            for (auto & exe_pipe : valid_exe_pipe)
            {
                if (exe_pipe->canAccept())
                {
                    ILOG("Sending instruction " << inst << " to exe_pipe " << exe_pipe->getName())
                    exe_pipe->insertInst(inst);
                    auto delete_iter = inst_itr++;
                    sent = true;
                    ready_queue_.erase(delete_iter);
                    popIssueQueue_(inst);
                    ++total_insts_issued_;
                    break;
                }
            }
            if (!sent)
            {
                ++inst_itr;
            }
        }
    }

    void IssueQueue::flushInst_(const FlushManager::FlushingCriteria & criteria)
    {
        uint32_t credits_to_send = 0;

        auto issue_queue_iter = issue_queue_.begin();
        while (issue_queue_iter != issue_queue_.end())
        {
            auto delete_iter = issue_queue_iter++;

            auto inst_ptr = *delete_iter;

            // Remove flushed instruction from issue queue and clear scoreboard
            // callbacks
            if (criteria.includedInFlush(inst_ptr))
            {
                issue_queue_.erase(delete_iter);

                scoreboard_views_->clearCallbacks(inst_ptr->getUniqueID());

                ++credits_to_send;

                ILOG("Flush Instruction ID: " << inst_ptr->getUniqueID() << " from issue queue");
            }
        }

        if (credits_to_send)
        {
            out_scheduler_credits_.send(credits_to_send, 0);

            ILOG("Flush " << credits_to_send << " instructions in issue queue!");
        }

        // Flush instructions in the ready queue
        auto ready_queue_iter = ready_queue_.begin();
        while (ready_queue_iter != ready_queue_.end())
        {
            auto delete_iter = ready_queue_iter++;
            auto inst_ptr = *delete_iter;
            if (criteria.includedInFlush(inst_ptr))
            {
                ready_queue_.erase(delete_iter);
                ILOG("Flush Instruction ID: " << inst_ptr->getUniqueID() << " from ready queue");
            }
        }
    }

    // Append instruction into issue queue
    void IssueQueue::appendIssueQueue_(const InstPtr & inst_ptr)
    {
        sparta_assert(issue_queue_.size() <= scheduler_size_,
                      "Appending issue queue causes overflows!");

        issue_queue_.emplace_back(inst_ptr);
    }

    // Pop completed instruction out of issue queue
    void IssueQueue::popIssueQueue_(const InstPtr & inst_ptr)
    {
        // Look for the instruction to be completed, and remove it from issue queue
        for (auto iter = issue_queue_.begin(); iter != issue_queue_.end(); iter++)
        {
            if (*iter == inst_ptr)
            {
                issue_queue_.erase(iter);
                out_scheduler_credits_.send(
                    1, 0); // send credit back to dispatch, we now have more room in IQ
                return;
            }
        }
        sparta_assert(false, "Attempt to complete instruction no longer exiting in issue queue!");
    }
} // namespace olympia