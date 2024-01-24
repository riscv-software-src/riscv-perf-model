// <Execute.cpp> -*- C++ -*-

#include "Execute.hpp"
#include "CoreUtils.hpp"
#include "IssueQueue.hpp"

namespace olympia
{
    Execute::Execute(sparta::TreeNode* node, const ExecuteParameterSet*) : sparta::Unit(node) {}

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
            const auto tgt_name = exe_unit_pair[0];
            const auto unit_count = exe_unit_pair[1];
            const auto exe_idx = (unsigned int)std::stoul(unit_count);
            sparta_assert(exe_idx > 0, "Expected more than 0 units! " << tgt_name);
            for (uint32_t unit_num = 0; unit_num < exe_idx; ++unit_num)
            {
                const std::string unit_name = tgt_name + std::to_string(unit_num);
                exe_pipe_tns_.emplace_back(new sparta::ResourceTreeNode(
                    node, unit_name, tgt_name, unit_num, std::string(unit_name + " Execution Pipe"),
                    &exe_pipe_fact_));
            }
        }

        issue_queue_topology_ =
            olympia::coreutils::getPipeTopology(node->getParent(), "issue_queue_topology");
        for (size_t i = 0; i < issue_queue_topology_.size(); ++i)
        {
            // loop through the issue_queue_topology and create a corresponding issue_queue sparta
            // unit
            std::string exe_units_string = "";
            for (auto exe_unit : issue_queue_topology_[i])
            {
                exe_units_string += exe_unit + ", ";
            }
            const std::string issue_queue_name = "iq" + std::to_string(i);
            // naming it for the target type it supports, i.e issue_queue_alu or issue_queue_fpu
            const std::string tgt_name =
                "IssueQueue_"
                + issue_queue_topology_[i][0].substr(0, issue_queue_topology_[i][0].size() - 1);
            issue_queues_.emplace_back(new sparta::ResourceTreeNode(
                node, issue_queue_name, tgt_name, i, exe_units_string + std::string("Issue Queue"),
                &issue_queue_fact_));
        }
    }

    void ExecuteFactory::bindLate(sparta::TreeNode* node)
    {
        /*
            For issue queue we need to establish mappings such that a mapping of target pipe to
           execution pipe in an issue queue is known such as: iq_0: "int": alu0, alu1 "div": alu1
                    "mul": alu3
            so when we have an instruction, we can get the target pipe of an instruction and lookup
           available execution units
        */
        std::unordered_map<std::string, int> exe_pipe_to_iq_number;

        for (size_t i = 0; i < issue_queues_.size(); ++i)
        {
            // loop through execution units in each definition of the issue queue topology
            // "alu0", "alu1"
            for (const auto & exe_name : issue_queue_topology_[i])
            {
                // we need to find the corresponding exe_pipe_ with the same name, search through
                // exe_pipe_tns_
                for (const auto & exe_pipe_tns : exe_pipe_tns_)
                {
                    olympia::ExecutePipe* exe_pipe =
                        exe_pipe_tns->getResourceAs<olympia::ExecutePipe*>();
                    const std::string exe_pipe_name = exe_pipe_tns->getName();
                    // check if the names match, then we have the correct exe_pipe_
                    if (exe_name == exe_pipe_name)
                    {
                        olympia::IssueQueue* issue_queue =
                            issue_queues_[i]->getResourceAs<olympia::IssueQueue*>();
                        // set in the issue_queue the corresponding exe_pipe
                        issue_queue->setExePipe(exe_pipe_name, exe_pipe);
                        // establish a mapping of execution_pipe type to which issue_queue number it
                        // is
                        exe_pipe_to_iq_number[exe_pipe_name] = i;
                    }
                }
            }
        }

        auto setup_issue_queue_tgts =
            [this, &exe_pipe_to_iq_number](sparta::TreeNode* node, std::string exe_unit)
        {
            std::string topology_string = "pipe_topology_" + exe_unit + "_pipes";
            auto pipe_topology =
                olympia::coreutils::getPipeTopology(node->getParent(), topology_string);

            for (size_t i = 0; i < pipe_topology.size(); ++i)
            {
                // loop through pipe topology, which defines pipes per execution unit:
                // ["INT", "MUL"] -> ALU0 supports pipes "INT", "MUL"
                // so we're looping on "INT" and "MUL" to set the mapping of:
                // "INT": ["alu0", "alu1"] in the issue_queue
                for (size_t j = 0; j < pipe_topology[i].size(); ++j)
                {
                    const auto pipe_name = pipe_topology[i][j];
                    const auto tgt_pipe = InstArchInfo::execution_pipe_map.find(pipe_name);
                    const auto exe_unit_name = exe_unit + std::to_string(i);
                    // iq_num is the issue queue number based on the execution name, as we are
                    // looping through the pipe types for a execution unit, we don't know which
                    // issue queue it maps to, unless we use a map
                    const auto iq_num = exe_pipe_to_iq_number.at(exe_unit_name);
                    olympia::IssueQueue* my_issue_queue =
                        issue_queues_[iq_num]->getResourceAs<olympia::IssueQueue*>();

                    my_issue_queue->setExePipeMapping(
                        tgt_pipe->second, my_issue_queue->getExePipes().at(exe_unit_name));
                }
            }
        };
        for (auto iq_type : core_types::issue_queue_types)
        {
            setup_issue_queue_tgts(node, iq_type);
        }
    }

    void ExecuteFactory::deleteSubtree(sparta::ResourceTreeNode*)
    {
        exe_pipe_tns_.clear();
        issue_queues_.clear();
    }

} // namespace olympia
