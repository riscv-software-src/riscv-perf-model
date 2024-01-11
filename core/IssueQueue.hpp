// <IssueQueue.hpp> -*- C++ -*-

#pragma once

#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/collection/Collectable.hpp"
#include "sparta/resources/Scoreboard.hpp"

#include "ExecutePipe.hpp"
#include "Inst.hpp"
#include "CoreTypes.hpp"
#include "FlushManager.hpp"

namespace olympia
{
    /**
     * \class IssueQueue
     * \brief Defines the stages for an issue queue
     */
    class IssueQueue: public sparta::Unit
    {
        // need to create a queue to hold instructions
        // need to know what pipes this IQ is mapped to
        // interface with ExecutePipe to be able to execute instruction
        // interface with dispatch
        public:
            class IssueQueueParameterSet : public sparta::ParameterSet
            {
            public:
                IssueQueueParameterSet(sparta::TreeNode* n) :
                    sparta::ParameterSet(n)
                { }
                PARAMETER(uint32_t, issue_queue_size, 10, "Size of Issue Queue")
            };
            IssueQueue(sparta::TreeNode * node, const IssueQueueParameterSet * p);
            static const char name[];
            void setExePipe(std::string exe_pipe_name, olympia::ExecutePipe* exe_pipe);
            void setExePipeMapping(InstArchInfo::TargetPipe tgt_pipe, olympia::ExecutePipe* exe_pipe);
            std::unordered_map<std::string, olympia::ExecutePipe*> getExePipes();
        private:
            /*
                {
                    "alu0": ptr
                    "alu1": ptr
                    ...

                }

                how to create: pipe_exe_pipe_mapping_
                    - just walk the parameters passed, which tells us which target types are valid for each exe_unit
                ex_inst -> "int"

                pipe_exe_pipe_mapping_["int"] --> (vector) [alu0, alu1, alu2]



                Flow:
                    ex_inst->getPipe() -> call this, find out what pipe type it needs
                    we look at all the IQs that support it, send it to the IQ with least amount of instructions
                    IQ then pops the front inst, we then call ex_inst->getPipe(), need to look at which execution units support that pipe

                    we need two mappings, one for dispatch to know which IQs support which TARGETs
                    one for IQs to know which execution units support which targets
            */
            // Scoreboards
            using ScoreboardViews = std::array<std::unique_ptr<sparta::ScoreboardView>, core_types::N_REGFILES>;
            ScoreboardViews scoreboard_views_;

            // Tick events
            sparta::SingleCycleUniqueEvent<> ev_issue_ready_inst_{&unit_event_set_, "issue_event",
                                                                CREATE_SPARTA_HANDLER(IssueQueue, sendReadyInsts_)};
            // Events used to issue and complete the instruction
            // sparta::UniqueEvent<> issue_inst_{&unit_event_set_, getName() + "_issue_inst",
            //     CREATE_SPARTA_HANDLER(IssueQueue, issueInst_)};
            void setupIssueQueue_();
            void getInstsFromDispatch_(const InstPtr&);
            void flushInst_(const FlushManager::FlushingCriteria & criteria);
            void sendReadyInsts_();
            void readyExeUnit_(const uint32_t &);
            
            sparta::DataInPort<InstQueue::value_type> in_execute_inst_ {&unit_port_set_, "in_execute_write", 1};
            // in_exe_pipe_done tells us an executon pipe is done
            sparta::DataInPort<uint32_t> in_exe_pipe_done_ {&unit_port_set_, "in_execute_pipe"};
            sparta::DataOutPort<uint32_t> out_scheduler_credits_{&unit_port_set_, "out_scheduler_credits"};
            sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_
            {&unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};
            // mapping used to lookup based on execution unit name
            // {"alu0": ExecutePipe0}
            std::unordered_map<std::string, olympia::ExecutePipe*> exe_pipes_;
            // mapping of target pipe -> vector of execute pipes
            // i.e {"INT", <ExecutePipe0, ExecutePipe1>}
            std::unordered_map<InstArchInfo::TargetPipe, std::vector<olympia::ExecutePipe*>> pipe_exe_pipe_mapping_;
            std::vector<olympia::ExecutePipe*> pipes_;
            std::vector<std::string> exe_unit_str_;
            const core_types::RegFile reg_file_;

            // Ready queue
            typedef std::list<InstPtr> ReadyQueue;
            ReadyQueue  ready_queue_;

            uint32_t issue_queue_size_;
            //std::string name_;
            //vector<std::string>> exe_units_; // execution units mapped to this issue queue
            // mapping of which pipes are mapped to which execution unit
            // "alu0": ["int", "mul"] -> alu0 can take pipe instructions "int" and "mul"
            //std::unordered_map<std::string, vector<std::string>> pipe_mappings_;

    };
    using IssueQueueFactory = sparta::ResourceFactory<olympia::IssueQueue,
                                                       olympia::IssueQueue::IssueQueueParameterSet>;
}