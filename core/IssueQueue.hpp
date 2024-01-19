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

#include "CoreTypes.hpp"
#include "ExecutePipe.hpp"
#include "FlushManager.hpp"
#include "Inst.hpp"

namespace olympia {
/**
 * \class IssueQueue
 * \brief Defines the stages for an issue queue
 */
class IssueQueue : public sparta::Unit {
  // Issue Queue interfaces between Dispatch and ExecutePipe and holds
  // instructions until an available ExecutePipe becomes ready Dispatch -> Issue
  // Queue -> ExecutePipe
public:
  class IssueQueueParameterSet : public sparta::ParameterSet {
  public:
    IssueQueueParameterSet(sparta::TreeNode *n) : sparta::ParameterSet(n) {}
    PARAMETER(uint32_t, scheduler_size, 8, "Scheduler queue size")
    PARAMETER(bool, in_order_issue, true, "Force in order issue")
  };

  IssueQueue(sparta::TreeNode *node, const IssueQueueParameterSet *p);
  static const char name[];
  void setExePipe(std::string exe_pipe_name, olympia::ExecutePipe *exe_pipe);
  void setExePipeMapping(InstArchInfo::TargetPipe tgt_pipe,
                         olympia::ExecutePipe *exe_pipe);
  std::unordered_map<std::string, olympia::ExecutePipe *> getExePipes();

private:
  /*
      {
          "alu0": ptr
          "alu1": ptr
          ...

      }

      how to create: pipe_exe_pipe_mapping_
          - just walk the parameters passed, which tells us which target types
     are valid for each exe_unit ex_inst -> "int"

      pipe_exe_pipe_mapping_["int"] --> (vector) [alu0, alu1, alu2]



      Flow:
          ex_inst->getPipe() -> call this, find out what pipe type it needs
          we look at all the IQs that support it, send it to the IQ with least
     amount of instructions IQ then pops the front inst, we then call
     ex_inst->getPipe(), need to look at which execution units support that pipe

          we need two mappings, one for dispatch to know which IQs support which
     TARGETs one for IQs to know which execution units support which targets
  */
  // Scoreboards
  using ScoreboardViews = std::array<std::unique_ptr<sparta::ScoreboardView>,
                                     core_types::N_REGFILES>;
  ScoreboardViews scoreboard_views_;

  // Tick events
  sparta::SingleCycleUniqueEvent<> ev_issue_ready_inst_{
      &unit_event_set_, "issue_event",
      CREATE_SPARTA_HANDLER(IssueQueue, sendReadyInsts_)};
  void setupIssueQueue_();
  void getInstsFromDispatch_(const InstPtr &);
  void flushInst_(const FlushManager::FlushingCriteria &criteria);
  void sendReadyInsts_();
  void readyExeUnit_(const uint32_t &);
  void appendIssueQueue_(const InstPtr &);
  void popIssueQueue_(const InstPtr &);
  void handleOperandIssueCheck_(const InstPtr &);
  void onROBTerminate_(const bool &val);
  void onStartingTeardown_() override;
  void dumpDebugContent_(std::ostream &output) const override final;

  sparta::DataInPort<InstQueue::value_type> in_execute_inst_{
      &unit_port_set_, "in_execute_write", 1};
  // in_exe_pipe_done tells us an executon pipe is done
  sparta::DataInPort<uint32_t> in_exe_pipe_done_{&unit_port_set_,
                                                 "in_execute_pipe"};
  sparta::DataOutPort<uint32_t> out_scheduler_credits_{&unit_port_set_,
                                                       "out_scheduler_credits"};
  sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_{
      &unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};
  // mapping used to lookup based on execution unit name
  // {"alu0": ExecutePipe0}
  std::unordered_map<std::string, olympia::ExecutePipe *> exe_pipes_;
  // mapping of target pipe -> vector of execute pipes
  // i.e {"INT", <ExecutePipe0, ExecutePipe1>}
  std::unordered_map<InstArchInfo::TargetPipe,
                     std::vector<olympia::ExecutePipe *>>
      pipe_exe_pipe_mapping_;
  // storage of what pipes are in this issue queues
  std::vector<olympia::ExecutePipe *> pipes_;
  std::vector<std::string> exe_unit_str_;
  const core_types::RegFile reg_file_;

  // Ready queue
  typedef std::list<InstPtr> ReadyQueue;
  ReadyQueue ready_queue_; // queue for instructions that have operands ready
  ReadyQueue issue_queue_; // instructions that have been sent from dispatch,
                           // not ready yet

  uint32_t scheduler_size_;
  bool in_order_issue_;
  sparta::collection::IterableCollector<std::list<InstPtr>>
      ready_queue_collector_{getContainer(), "scheduler_queue", &ready_queue_,
                             scheduler_size_};
  sparta::Counter total_insts_issued_{getStatisticSet(), "total_insts_issued",
                                      "Total instructions issued",
                                      sparta::Counter::COUNT_NORMAL};
  bool rob_stopped_simulation_ = false;
  friend class IssueQueueTester;
};

using IssueQueueFactory =
    sparta::ResourceFactory<olympia::IssueQueue,
                            olympia::IssueQueue::IssueQueueParameterSet>;
} // namespace olympia