// <Execute.cpp> -*- C++ -*-

#include "Execute.hpp"
#include "CoreUtils.hpp"

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
    }


    void ExecuteFactory::bindLate (sparta::TreeNode *node)
    {
        auto bind_ports =
            [node] (const std::string & left, const std::string & right) {
                sparta::bind(node->getParent()->getChildAs<sparta::Port>(left),
                             node->getParent()->getChildAs<sparta::Port>(right));
            };

        // Bind all of the pipes created to Dispatch and Flushmanager
        for(auto & exe_pipe_rtn : exe_pipe_tns_)
        {
            const std::string exe_credits_out = "execute." + exe_pipe_rtn->getName() +
                ".ports.out_scheduler_credits";
            const std::string disp_credits_in = "dispatch.ports.in_" + exe_pipe_rtn->getName() + "_credits";
            bind_ports(exe_credits_out, disp_credits_in);

            const std::string exe_inst_in   = "execute." + exe_pipe_rtn->getName() + ".ports.in_execute_write";
            const std::string disp_inst_out = "dispatch.ports.out_" + exe_pipe_rtn->getName() + "_write";
            bind_ports(exe_inst_in, disp_inst_out);

            const std::string exe_flush_in = "execute." +  exe_pipe_rtn->getName() + ".ports.in_reorder_flush";;
            const std::string flush_manager = "flushmanager.ports.out_retire_flush";
            bind_ports(exe_flush_in, flush_manager);
        }
    }
}
