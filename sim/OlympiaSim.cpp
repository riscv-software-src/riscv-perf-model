// <OlympiaSim.cpp> -*- C++ -*-

#include <iostream>

#include "OlympiaSim.hpp"

#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/StringUtils.hpp"

#include "CPUFactory.hpp"
#include "SimulationConfiguration.hpp"

#include "OlympiaAllocators.hpp"

OlympiaSim::OlympiaSim(sparta::Scheduler & scheduler,
                       const uint32_t num_cores,
                       const std::string workload,
                       const uint64_t instruction_limit,
                       const bool show_factories) :
    sparta::app::Simulation("sparta_olympia", &scheduler),
    num_cores_(num_cores),
    workload_(workload),
    instruction_limit_(instruction_limit),
    show_factories_(show_factories)
{
    // Set up the CPU Resource Factory to be available through ResourceTreeNode
    getResourceSet()->addResourceFactory<olympia::CPUFactory>();
}

OlympiaSim::~OlympiaSim()
{
    getRoot()->enterTeardown(); // Allow deletion of nodes without error now
}

//! Get the resource factory needed to build and bind the tree
auto OlympiaSim::getCPUFactory_() -> olympia::CPUFactory*{
    auto sparta_res_factory = getResourceSet()->getResourceFactory("cpu");
    auto cpu_factory = dynamic_cast<olympia::CPUFactory*>(sparta_res_factory);
    return cpu_factory;
}

void OlympiaSim::buildTree_()
{
    // TREE_BUILDING Phase.  See sparta::PhasedObject::TreePhase
    auto cpu_factory = getCPUFactory_();

    // Create the common Allocators
    allocators_tn_.reset(new olympia::OlympiaAllocators(getRoot()));

    // Create a single CPU
    sparta::ResourceTreeNode* cpu_tn = new sparta::ResourceTreeNode(getRoot(),
                                                                    "cpu",
                                                                    sparta::TreeNode::GROUP_NAME_NONE,
                                                                    sparta::TreeNode::GROUP_IDX_NONE,
                                                                    "CPU Node",
                                                                    cpu_factory);

    // Set the cpu topology that will be built
    auto topology = cpu_tn->getParameterSet()->getParameter("topology");
    cpu_factory->setTopology(topology->getValueAsString(), num_cores_);

    // You _can_ use sparta::app::Simulation's to_delete_ vector and
    // have the simulation class delete it for you.  However, by doing
    // that, the Allocators object _will be destroyed_ before the CPU
    // TN resulting in a seg fault at the end of simulation.
    cpu_tn_to_delete_.reset(cpu_tn);

    cpu_tn->addExtensionFactory("simulation_configuration", [=](){return new olympia::SimulationConfiguration;});

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
