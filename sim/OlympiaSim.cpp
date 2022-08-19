// <OlympiaSim.cpp> -*- C++ -*-

#include <iostream>

#include "OlympiaSim.hpp"

#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/trigger/ContextCounterTrigger.hpp"
#include "sparta/utils/StringUtils.hpp"

#include "Core.hpp"
#include "CPUFactory.hpp"
#include "Fetch.hpp"
#include "Decode.hpp"
#include "Rename.hpp"
#include "Dispatch.hpp"
#include "Execute.hpp"
#include "LSU.hpp"
#include "ROB.hpp"
#include "FlushManager.hpp"
#include "Preloader.hpp"
#include "SimulationConfiguration.hpp"

#include "BIU.hpp"
#include "MSS.hpp"

OlympiaSim::OlympiaSim(const std::string& topology,
                       sparta::Scheduler & scheduler,
                       const uint32_t num_cores,
                       const std::string workload,
                       const uint64_t instruction_limit,
                       const bool show_factories) :
    sparta::app::Simulation("sparta_olympia_core", &scheduler),
    cpu_topology_(topology),
    num_cores_(num_cores),
    workload_(workload),
    instruction_limit_(instruction_limit),
    show_factories_(show_factories)
{
    // Set up the CPU Resource Factory to be available through ResourceTreeNode
    getResourceSet()->addResourceFactory<olympia_core::CPUFactory>();
}

OlympiaSim::~OlympiaSim()
{
    getRoot()->enterTeardown(); // Allow deletion of nodes without error now
}

//! Get the resource factory needed to build and bind the tree
auto OlympiaSim::getCPUFactory_() -> olympia_core::CPUFactory*{
    auto sparta_res_factory = getResourceSet()->getResourceFactory("cpu");
    auto cpu_factory = dynamic_cast<olympia_core::CPUFactory*>(sparta_res_factory);
    return cpu_factory;
}

void OlympiaSim::buildTree_()
{
    // TREE_BUILDING Phase.  See sparta::PhasedObject::TreePhase
    auto cpu_factory = getCPUFactory_();

    // Set the cpu topology that will be built
    cpu_factory->setTopology(cpu_topology_, num_cores_);

    // Create a single CPU
    sparta::ResourceTreeNode* cpu_tn = new sparta::ResourceTreeNode(getRoot(),
                                                                    "cpu",
                                                                    sparta::TreeNode::GROUP_NAME_NONE,
                                                                    sparta::TreeNode::GROUP_IDX_NONE,
                                                                    "CPU Node",
                                                                    cpu_factory);
    to_delete_.emplace_back(cpu_tn);

    cpu_tn->addExtensionFactory("simulation_configuration", [=](){return new olympia_core::SimulationConfiguration;});

    // Tell the factory to build the resources now
    cpu_factory->buildTree(getRoot());

    // Set the workload in the simulation configuration
    auto extension = sparta::notNull(cpu_tn->getExtension("simulation_configuration"));
    auto workload  = extension->getParameters()->getParameter("workload");
    workload->setValueFromString(workload_);

    // Print the registered factories for debug
    if(show_factories_){
        std::cout << "Registered factories: \n";
        for(const auto& f : getCPUFactory_()->getResourceNames()){
            std::cout << "\t" << f << std::endl;
        }
    }
}

void OlympiaSim::configureTree_()
{
    // In TREE_CONFIGURING phase
    // Configuration from command line is already applied

    sparta::ParameterBase* max_instrs =
        getRoot()->getChildAs<sparta::ParameterBase>("cpu.core0.rob.params.num_insts_to_retire");

    // Safely assign as string for now in case parameter type changes.
    // Direct integer assignment without knowing parameter type is not yet available through C++ API
    if(instruction_limit_ != 0){
        max_instrs->setValueFromString(sparta::utils::uint64_to_str(instruction_limit_));
    }
}

void OlympiaSim::bindTree_()
{
    // In TREE_FINALIZED phase
    // Tree is finalized. Taps placed. No new nodes at this point
    // Bind appropriate ports

    //Tell the factory to bind all units
    auto cpu_factory = getCPUFactory_();
    cpu_factory->bindTree(getRoot());
}


// The Sparta framework's coammand line option '-i <insts>' needs to
// know which counter this option is assoicated with.  The framework
// will call this function asking for that counter to associate.
// Currently only CSEM_INSTRUCTIONS is supported, but more can be added...
const sparta::CounterBase* OlympiaSim::findSemanticCounter_(CounterSemantic sem) const {
    switch(sem){
        case CSEM_INSTRUCTIONS:
            return getRoot()->getChildAs<const sparta::CounterBase>("cpu.core0.rob.stats.total_number_retired");
            break;
        default:
            return nullptr;
    }
}
