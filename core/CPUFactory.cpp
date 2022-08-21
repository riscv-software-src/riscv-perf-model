// <CPUFactory.cpp> -*- C++ -*-


#include "CPUFactory.hpp"
#include <string>
#include <algorithm>

/**
 * @brief Constructor for CPUFactory
 */
olympia_core::CPUFactory::CPUFactory() :
    sparta::ResourceFactory<olympia_core::CPU, olympia_core::CPU::CPUParameterSet>(){}

/**
 * @brief Destructor for CPUFactory
 */
olympia_core::CPUFactory::~CPUFactory() = default;

/**
 * @brief Set the user-defined topology for this microarchitecture
 */
auto olympia_core::CPUFactory::setTopology(const std::string& topology,
                                           const uint32_t num_cores) -> void{
    sparta_assert(!topology_);
    topology_.reset(olympia_core::CPUTopology::allocateTopology(topology));
    topology_->setName(topology);
    topology_->setNumCores(num_cores);
}

/**
 * @brief Implemenation : Build the device tree by instantiating resource nodes
 */
auto olympia_core::CPUFactory::buildTree_(sparta::RootTreeNode* root_node,
                                          const std::vector<olympia_core::CPUTopology::UnitInfo>& units) -> void
{
    std::string parent_name, human_name, node_name, replace_with;
    for(std::size_t num_of_cores = 0; num_of_cores < topology_->num_cores; ++num_of_cores){
        for(const auto& unit : units){
            parent_name = unit.parent_name;
            node_name = unit.name;
            human_name = unit.human_name;
            replace_with = std::to_string(num_of_cores);
            std::replace(parent_name.begin(), parent_name.end(), to_replace_, *replace_with.c_str());
            std::replace(node_name.begin(), node_name.end(), to_replace_, *replace_with.c_str());
            std::replace(human_name.begin(), human_name.end(), to_replace_, *replace_with.c_str());
            auto parent_node = root_node->getChildAs<sparta::TreeNode>(parent_name);
            auto rtn = new sparta::ResourceTreeNode(parent_node,
                                                    node_name,
                                                    unit.group_name,
                                                    unit.group_id,
                                                    human_name,
                                                    unit.factory);
            if(unit.is_private_subtree){
                rtn->makeSubtreePrivate();
                private_nodes_.emplace_back(rtn);
            }
            to_delete_.emplace_back(rtn);
            resource_names_.emplace_back(node_name);
        }
    }
}

/**
 * @brief Implementation : Bind all the ports between different units and set TLBs and preload
 */
auto olympia_core::CPUFactory::bindTree_(sparta::RootTreeNode* root_node,
                                         const std::vector<olympia_core::CPUTopology::PortConnectionInfo>& ports) -> void
{
    std::string out_port_name, in_port_name,replace_with;
    for(std::size_t num_of_cores = 0; num_of_cores < topology_->num_cores; ++num_of_cores){
        for(const auto& port : ports){
            out_port_name = port.output_port_name;
            in_port_name = port.input_port_name;
            replace_with = std::to_string(num_of_cores);
            std::replace(out_port_name.begin(), out_port_name.end(), to_replace_, *replace_with.c_str());
            std::replace(in_port_name.begin(), in_port_name.end(), to_replace_, *replace_with.c_str());
            sparta::bind(root_node->getChildAs<sparta::Port>(out_port_name),
                         root_node->getChildAs<sparta::Port>(in_port_name));
        }

        // Set the TLBs and preload
        auto core_tree_node = root_node->getChild(std::string("cpu.core") +
                                                  std::to_string(num_of_cores));
        sparta_assert(core_tree_node != nullptr);
        (core_tree_node->getChild("lsu")->getResourceAs<olympia_core::LSU>())->
            setTLB(*private_nodes_.at(num_of_cores)->getResourceAs<olympia_core::SimpleTLB>());
        (core_tree_node->getChild("preloader")->getResourceAs<olympia_core::Preloader>())->
            preload();
    }
}

/**
 * @brief Build the device tree by instantiating resource nodes
 */
auto olympia_core::CPUFactory::buildTree(sparta::RootTreeNode* root_node) -> void
{
    sparta_assert(topology_);
    buildTree_(root_node, topology_->units);
}

/**
 * @brief Bind all the ports between different units and set TLBs and preload
 */
auto olympia_core::CPUFactory::bindTree(sparta::RootTreeNode* root_node) -> void
{
    sparta_assert(topology_);
    bindTree_(root_node, topology_->port_connections);
}

/**
 * @brief Get the list of resources instantiated in this topology
 */
auto olympia_core::CPUFactory::getResourceNames() const -> const std::vector<std::string>&
{
    return resource_names_;
}
