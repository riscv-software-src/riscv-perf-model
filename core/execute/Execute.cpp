// <Execute.cpp> -*- C++ -*-
#include "execute/Execute.hpp"
#include "CoreUtils.hpp"
#include "execute/IssueQueue.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace olympia
{
    Execute::Execute(sparta::TreeNode* node, const ExecuteParameterSet*) : sparta::Unit(node) {}

    void ExecuteFactory::onConfiguring(sparta::ResourceTreeNode* node)
    {
        issue_queue_to_pipe_map_ =
            olympia::coreutils::getPipeTopology(node->getParent(), "issue_queue_to_pipe_map");
        const auto issue_queue_rename =
            olympia::coreutils::getPipeTopology(node->getParent(), "issue_queue_rename");
        // create issue queue sparta units
        for (size_t iq_idx = 0; iq_idx < issue_queue_to_pipe_map_.size(); ++iq_idx)
        {
            std::string issue_queue_name = "iq" + std::to_string(iq_idx);
            if (issue_queue_rename.size() > 0)
            {
                sparta_assert(issue_queue_rename[iq_idx][0] == issue_queue_name,
                              "Rename mapping for issue queue is not in order or the original unit "
                              "name is not equal to the unit name, check spelling!")
                    issue_queue_name = issue_queue_rename[iq_idx][1];
            }
            const std::string tgt_name = "Issue_Queue";
            issue_queues_.emplace_back(
                new sparta::ResourceTreeNode(node, issue_queue_name, tgt_name, iq_idx,
                                             std::string("Issue_Queue"), &issue_queue_fact_));
        }

        std::vector<int> pipe_to_iq_mapping;
        // map of which pipe goes to which issue queue
        // "int": "iq0", "iq1"
        for (size_t iq_num = 0; iq_num < issue_queue_to_pipe_map_.size(); ++iq_num)
        {
            const auto iq = issue_queue_to_pipe_map_[iq_num];
            const auto pipe_target_start = std::stoi(iq[0]);
            auto pipe_target_end = pipe_target_start;
            // we could have a 1 to 1 mapping or multiple, so have
            // to check accordingly
            if (iq.size() > 1)
            {
                pipe_target_end = std::stoi(iq[1]);
            }
            pipe_target_end++;
            for (int pipe_idx = pipe_target_start; pipe_idx < pipe_target_end; ++pipe_idx)
            {
                pipe_to_iq_mapping.push_back(iq_num);
            }
        }
        const auto exe_pipe_rename =
            olympia::coreutils::getPipeTopology(node->getParent(), "exe_pipe_rename");
        const auto pipelines = olympia::coreutils::getPipeTopology(node->getParent(), "pipelines");
        for (size_t pipeidx = 0; pipeidx < pipelines.size(); ++pipeidx)
        {
            std::string iq_name = "iq" + std::to_string(pipe_to_iq_mapping[pipeidx]);
            if (issue_queue_rename.size() > 0)
            {
                sparta_assert(issue_queue_rename[pipe_to_iq_mapping[pipeidx]][0] == iq_name,
                              "Rename mapping for issue queue is not in order or the original unit "
                              "name is not equal to the unit name, check spelling!") iq_name =
                    issue_queue_rename[pipe_to_iq_mapping[pipeidx]][1];
            }
            const std::string tgt_name = iq_name + "_group";
            std::string unit_name = "exe" + std::to_string(pipeidx);
            if (exe_pipe_rename.size() > 0)
            {
                sparta_assert(exe_pipe_rename[pipeidx][0] == unit_name,
                              "Rename mapping for execution is not in order or the original unit "
                              "name is not equal to the unit name, check spelling!") unit_name =
                    exe_pipe_rename[pipeidx][1];
            }
            exe_pipe_tns_.emplace_back(new sparta::ResourceTreeNode(
                node, unit_name, tgt_name, pipeidx, std::string(unit_name + " Execution Pipe"),
                &exe_pipe_fact_));
            exe_pipe_tns_.back()->getParameterSet()->getParameter("iq_name")->setValueFromString(
                iq_name);
            // if execution has branch pipe target
            auto pipe_targets = pipelines[pipeidx];
            if (std::find(pipe_targets.begin(), pipe_targets.end(), "br") != pipe_targets.end())
            {
                exe_pipe_tns_.back()
                    ->getParameterSet()
                    ->getParameter("contains_branch_unit")
                    ->setValueFromString("true");
            }
        }
    }

    void ExecuteFactory::bindLate(sparta::TreeNode* node)
    {
        /*
            For issue queue we need to establish mappings such that a mapping of target pipe to
           execution pipe in an issue queue is known such as:
                    iq_0:
                    "int": exe0, exe1
                    "div": exe1
                    "mul": exe2
            so when we have an instruction, we can get the target pipe of an instruction and lookup
           available execution units
        */
        const auto exe_pipe_rename =
            olympia::coreutils::getPipeTopology(node->getParent(), "exe_pipe_rename");
        auto pipelines = olympia::coreutils::getPipeTopology(node->getParent(), "pipelines");
        std::unordered_map<std::string, int> exe_pipe_to_iq_number;

        for (size_t iq_num = 0; iq_num < issue_queue_to_pipe_map_.size(); ++iq_num)
        {
            auto iq = issue_queue_to_pipe_map_[iq_num];
            auto pipe_target_start = std::stoi(iq[0]);
            auto pipe_target_end = pipe_target_start;
            if (iq.size() > 1)
            {
                pipe_target_end = std::stoi(iq[1]);
            }
            pipe_target_end++;
            for (int pipe_idx = pipe_target_start; pipe_idx < pipe_target_end; ++pipe_idx)
            {
                std::string exe_name = "exe" + std::to_string(pipe_idx);
                if (exe_pipe_rename.size() > 0)
                {
                    sparta_assert(exe_pipe_rename[pipe_idx][0] == exe_name,
                                  "Rename mapping for execution is not in order or the original "
                                  "unit name is not equal to the unit name, check spelling!")
                        exe_name = exe_pipe_rename[pipe_idx][1];
                }

                for (const auto & exe_pipe_tns : exe_pipe_tns_)
                {
                    olympia::ExecutePipe* exe_pipe =
                        exe_pipe_tns->getResourceAs<olympia::ExecutePipe*>();
                    const std::string exe_pipe_name = exe_pipe_tns->getName();
                    // check if the names match, then we have the correct exe_pipe_
                    if (exe_name == exe_pipe_name)
                    {
                        olympia::IssueQueue* issue_queue =
                            issue_queues_[iq_num]->getResourceAs<olympia::IssueQueue*>();
                        // set in the issue_queue the corresponding exe_pipe
                        issue_queue->setExePipe(exe_pipe_name, exe_pipe);
                        // establish a mapping of execution_pipe type to which issue_queue number it
                        // is
                        exe_pipe_to_iq_number[exe_pipe_name] = iq_num;
                    }
                }
            }
        }
        // need to create a index that tells which pipe # each pipeline is i.e
        /*
            ["int"], # exe0
            ["int", "div"], # exe1
            ["int", "mul"], exe2
            ["int", "mul", "i2f", "cmov"], exe3
            ["int"], exe4
            ["int"], exe5
            ["float", "faddsub", "fmac"], exe6
            ["float", "f2i"], exe7
            ["br"], exe8
            ["br"] exe9
        */
        for (auto iq : issue_queue_to_pipe_map_)
        {
            auto pipe_target_start = std::stoi(iq[0]);
            // check if issue queue has more than 1 pipe
            auto pipe_target_end = pipe_target_start;
            if (iq.size() > 1)
            {
                pipe_target_end = std::stoi(iq[1]);
            }
            pipe_target_end++;
            for (int pipe_idx = pipe_target_start; pipe_idx < pipe_target_end; ++pipe_idx)
            {
                for (size_t pipe_target_idx = 0; pipe_target_idx < pipelines[pipe_idx].size();
                     ++pipe_target_idx)
                {
                    const auto pipe_name = pipelines[pipe_idx][pipe_target_idx];
                    const auto tgt_pipe = InstArchInfo::execution_pipe_map.find(pipe_name);
                    // const auto exe_unit_name = "exe" + std::to_string(pipe_idx);
                    auto exe_unit_name = "exe" + std::to_string(pipe_idx);
                    if (exe_pipe_rename.size() > 0)
                    {
                        sparta_assert(
                            exe_pipe_rename[pipe_idx][0] == exe_unit_name,
                            "Rename mapping for execution is not in order or the original unit "
                            "name is not equal to the unit name, check spelling!") exe_unit_name =
                            exe_pipe_rename[pipe_idx][1];
                    }
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
        }
    }

    void ExecuteFactory::deleteSubtree(sparta::ResourceTreeNode*)
    {
        exe_pipe_tns_.clear();
        issue_queues_.clear();
    }

} // namespace olympia
