// <IssueQueue.hpp> -*- C++ -*-

#pragma once

#include "sparta/collection/Collectable.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/resources/Scoreboard.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Unit.hpp"

#include "sparta/resources/PriorityQueue.hpp"

#include "CoreTypes.hpp"
#include "ExecutePipe.hpp"
#include "FlushManager.hpp"
#include "Inst.hpp"

namespace olympia
{

    /**
     * \class IssueQueue
     * \brief Defines the stages for an issue queue
     */
    class IssueQueue : public sparta::Unit
    {
        // Issue Queue interfaces between Dispatch and ExecutePipe and holds
        // instructions until an available ExecutePipe becomes ready Dispatch -> Issue
        // Queue -> ExecutePipe
      public:
        class IssueQueueParameterSet : public sparta::ParameterSet
        {
          public:
            IssueQueueParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}
            PARAMETER(uint32_t, scheduler_size, 8, "Scheduler queue size")
            PARAMETER(bool, in_order_issue, true, "Force in order issue")
        };

        class IssueQueueSorter
        {
          public:
            IssueQueueSorter() {}

            IssueQueueSorter(IssueQueueSorter* sorter) : dyn_sorter_(sorter) {}

            bool operator()(const InstPtr existing, const InstPtr to_be_inserted) const
            {
                return dyn_sorter_->choose(existing, to_be_inserted);
            }

            bool choose(const InstPtr existing, const InstPtr to_be_inserted)
            {
                if (fifo_)
                {
                    // we just want to insert it in the back for fifo order
                    return false;
                }
                else
                {
                    // age based insertion
                    return existing->getUniqueID() > to_be_inserted->getUniqueID();
                }
            }

            void setSorting(const bool sort_type) { fifo_ = sort_type; }

          private:
            bool fifo_ = true;
            IssueQueueSorter* dyn_sorter_ = this;
        };

        IssueQueue(sparta::TreeNode* node, const IssueQueueParameterSet* p);
        static const char name[];
        void setExePipe(const std::string & exe_pipe_name, olympia::ExecutePipe* exe_pipe);
        void setExePipeMapping(const InstArchInfo::TargetPipe tgt_pipe,
                               olympia::ExecutePipe* exe_pipe);
        typedef std::unordered_map<std::string, olympia::ExecutePipe*> StringToExePipe;

        StringToExePipe const getExePipes() { return exe_pipes_; };

      private:
        // Scoreboards
        using ScoreboardViews =
            std::unique_ptr<sparta::ScoreboardView>;
        ScoreboardViews scoreboard_views_;

        // Tick events
        sparta::SingleCycleUniqueEvent<> ev_issue_ready_inst_{
            &unit_event_set_, "issue_event", CREATE_SPARTA_HANDLER(IssueQueue, sendReadyInsts_)};
        void setupIssueQueue_();
        void receiveInstsFromDispatch_(const InstPtr &);
        void flushInst_(const FlushManager::FlushingCriteria & criteria);
        void sendReadyInsts_();
        void readyExeUnit_(const uint32_t &);
        void appendIssueQueue_(const InstPtr &);
        void popIssueQueue_(const InstPtr &);
        void handleOperandIssueCheck_(const InstPtr &);
        void onROBTerminate_(const bool & val);
        void onStartingTeardown_() override;
        void dumpDebugContent_(std::ostream & output) const override final;

        sparta::DataInPort<InstQueue::value_type> in_execute_inst_{&unit_port_set_,
                                                                   "in_execute_write", 1};
        // in_exe_pipe_done tells us an execution pipe is done
        sparta::DataInPort<uint32_t> in_exe_pipe_done_{&unit_port_set_, "in_execute_pipe"};
        sparta::DataOutPort<uint32_t> out_scheduler_credits_{&unit_port_set_,
                                                             "out_scheduler_credits"};
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_{
            &unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};
        // mapping used to lookup based on execution unit name
        // {"alu0": ExecutePipe0}
        StringToExePipe exe_pipes_;
        // mapping of target pipe -> vector of execute pipes
        // i.e {"INT", <ExecutePipe0, ExecutePipe1>}
        std::unordered_map<InstArchInfo::TargetPipe, std::vector<olympia::ExecutePipe*>>
            pipe_exe_pipe_mapping_;
        // storage of what pipes are in this issue queues
        std::vector<olympia::ExecutePipe*> pipes_;
        std::vector<std::string> exe_unit_str_;
        const core_types::RegFile reg_file_;
        IssueQueueSorter iq_sorter;
        sparta::PriorityQueue<InstPtr, IssueQueueSorter>
            ready_queue_;                // queue for instructions that have operands ready
        std::list<InstPtr> issue_queue_; // instructions that have been sent from dispatch,
                                         // not ready yet

        const uint32_t scheduler_size_;
        const bool in_order_issue_;
        // sparta::collection::IterableCollector<sparta::PriorityQueue<InstPtr, IssueQueueSorter>>
        //     ready_queue_collector_{getContainer(), "scheduler_queue", &IssueQueue::ready_queue_,
        //                            scheduler_size_};
        sparta::Counter total_insts_issued_{getStatisticSet(), "total_insts_issued",
                                            "Total instructions issued",
                                            sparta::Counter::COUNT_NORMAL};
        bool rob_stopped_simulation_ = false;
        friend class IssueQueueTester;
    };

    using IssueQueueFactory =
        sparta::ResourceFactory<olympia::IssueQueue, olympia::IssueQueue::IssueQueueParameterSet>;
    class IssueQueueTester;
} // namespace olympia