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
}
