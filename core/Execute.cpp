// <Execute.cpp> -*- C++ -*-

#include "Execute.hpp"
#include "CoreUtils.hpp"
#include "IssueQueue.hpp"

namespace olympia
{
    Execute::Execute(sparta::TreeNode * node, const ExecuteParameterSet *) :
        sparta::Unit(node)
    {}

    void ExecuteFactory::onConfiguring(sparta::ResourceTreeNode* node)
    {
        auto execution_topology = olympia::coreutils::getExecutionTopology(node->getParent());
        for (auto exe_unit_pair : execution_topology)
        {
            // Go through the core topology extension and create a
            // pipe for each entry.  For example:
            //    ["alu", "2"] will create 2 tree nodes:
            //
            //    execute.alu0
            //    execute.alu1
            //
            // both of type ExecutePipe
            //
            const auto tgt_name   = exe_unit_pair[0];
            const auto unit_count = exe_unit_pair[1];
            const auto exe_idx    = (unsigned int) std::stoul(unit_count);
            sparta_assert(exe_idx > 0, "Expected more than 0 units! " << tgt_name);
            for(uint32_t unit_num = 0; unit_num < exe_idx; ++unit_num)
            {
                const std::string unit_name = tgt_name + std::to_string(unit_num);
                exe_pipe_tns_.emplace_back(new sparta::ResourceTreeNode(node,
                                                                        unit_name,
                                                                        tgt_name,
                                                                        unit_num,
                                                                        std::string(unit_name + " Execution Pipe"),
                                                                        &exe_pipe_fact_));
            }
        }

        issue_queue_topology_ = olympia::coreutils::getPipeTopology(node->getParent(), "issue_queue_topology");
        for(size_t i = 0; i < issue_queue_topology_.size(); ++i){
            // loop through the issue_queue_topology and create a corresponding issue_queue sparta unit
            std::string exe_units_string = "";
            for(auto exe_unit: issue_queue_topology_[i]){
                exe_units_string += exe_unit + ", ";
            }
            const std::string issue_queue_name = "iq" + std::to_string(i);
            const std::string tgt_name = "IssueQueue_" + issue_queue_topology_[i][0].substr(0, issue_queue_topology_[i][0].size()-1);
            //const std::string tgt_name = issue_queue_topology_[i][0].substr(0, issue_queue_topology_[i][0].size()-1);
            auto iq_node = new sparta::ResourceTreeNode(node,
                                                        issue_queue_name,
                                                        tgt_name,
                                                        i,
                                                        exe_units_string + std::string("Issue Queue"),
                                                        &issue_queue_fact_);
            // auto iq_resource = iq_node->getResourceAs<olympia::IssueQueue*>();
            issue_queues_.emplace_back(iq_node);
        }
        
        // loop through and create N IssueQueues

    }
    void ExecuteFactory::bindLate(sparta::TreeNode* node){

        std::unordered_map<std::string, int> reverse_map;
        olympia::ExecutePipe* exe_pipe = exe_pipe_tns_[0]->getResourceAs<olympia::ExecutePipe*>();
        auto exe_pipe_name = exe_pipe->getName();
        for(size_t i = 0; i < issue_queues_.size(); i++){
            //auto & b = issue_queues_[i];
            for(auto alu_name : issue_queue_topology_[i]){
                for(auto & exe_pipe_tns: exe_pipe_tns_){
                    olympia::ExecutePipe* exe_pipe = exe_pipe_tns->getResourceAs<olympia::ExecutePipe*>();
                    std::string exe_pipe_name = exe_pipe->getName();
                    if(alu_name == exe_pipe_name){
                        olympia::IssueQueue* my_issue_queue = issue_queues_[i]->getResourceAs<olympia::IssueQueue*>();
                        my_issue_queue->setExePipe(exe_pipe_name, exe_pipe);
                        reverse_map[exe_pipe_name] = i;
                    }
                }
            }
        }

        auto setup_issue_queue_tgts = [this, reverse_map](sparta::TreeNode* node, std::string exe_unit){
            std::string topology_string = "pipe_topology_" + exe_unit + "_pipes";
            auto pipe_topology = olympia::coreutils::getPipeTopology(node->getParent(), topology_string);
        
        
            // allocate alu target mappings
            for(size_t i = 0; i < pipe_topology.size(); ++i){
                for(size_t j = 0; j < pipe_topology[i].size(); ++j){
                    std::string pipe_name = pipe_topology[i][j];
                    const auto tgt_pipe = InstArchInfo::execution_pipe_map.find(pipe_name);
                    std::string exe_unit_name = exe_unit + std::to_string(i);
                    // alu0
                    int iq_num = reverse_map.find(exe_unit_name)->second;
                    olympia::IssueQueue* my_issue_queue = issue_queues_[iq_num]->getResourceAs<olympia::IssueQueue*>();
                    const auto exe_pipes = my_issue_queue->getExePipes();
                    const auto exe_pipe = exe_pipes.find(exe_unit_name);
                    
                    my_issue_queue->setExePipeMapping(tgt_pipe->second, exe_pipe->second);
                }
            }
        };
        setup_issue_queue_tgts(node, "alu");
        setup_issue_queue_tgts(node, "fpu");
        setup_issue_queue_tgts(node, "br");
    }
    void ExecuteFactory::deleteSubtree(sparta::ResourceTreeNode*){
        exe_pipe_tns_.clear();
        issue_queues_.clear();
    }

}
