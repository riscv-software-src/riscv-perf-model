// <CoreUtils.hpp> -*- C++ -*-

#pragma once

#include <vector>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/Utils.hpp"

#include "CPUTopology.hpp"

namespace olympia::coreutils
{
    // Get the extension 'execution_topology' from the CoreExtension
    // extension.  The node passed is expected to be the node
    // containing the extension (for example, core0)
    inline auto getExecutionTopology(sparta::TreeNode * node)
    {
        auto core_extension           = node->getExtension(olympia::CoreExtensions::name);
        auto core_extension_params    = sparta::notNull(core_extension)->getParameters();
        auto execution_topology_param = sparta::notNull(core_extension_params)->getParameter("execution_topology");
        return sparta::notNull(execution_topology_param)->getValueAs<olympia::CoreExtensions::ExecutionTopology>();
    }
}
