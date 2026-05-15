
#include <string>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/functional/Register.hpp"

#include "Core.hpp"

namespace olympia
{
    Core::Core(sparta::TreeNode * node, // TreeNode which owns this. Publish child nodes to this
               const CoreParameterSet * p) : // Core::ParameterSet, not generic SPARTA set
        sparta::Unit(node)
    {
        // Now parameters and ports are fixed and sparta device tree is
        // now finalizing, so the parameters and ports can be used to
        // initialize this unit once and permanently.  The TreeNode
        // arg node is the TreeNode which constructed this resource.
        // In this constructor (only), we have the opportunity to add
        // more TreeNodes as children of this node, such as
        // RegisterSet, CounterSet, Register, Counter,
        // Register::Field, etc.  No new ResourceTreeNodes may be
        // added, however.  This unit's clock can be derived from
        // node->getClock().  Child resources of this node who do not
        // have their own nodes could examine the parameters argument
        // here and attach counters to this node's CounterSet.
    }

}
