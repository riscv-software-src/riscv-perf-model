// <CPUTopology.h> -*- C++ -*-


#pragma once

#include <memory>

#include "CPU.hpp"
#include "CPUFactories.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/simulation/ResourceTreeNode.hpp"
#include "sparta/simulation/TreeNodeExtensions.hpp"

namespace olympia{

    /**
     * @file  CPUTopology.h
     * @brief CPUTopology will act as the place where a user-defined topology
     *        is actually written. This class has structures for holding the
     *        required tree nodes and details about its parents nodes, names,
     *        groups, ids and whether it should be a private node or not.
     *
     * CPUTopology unit will
     * 1. Contain the nuts and bolts needed by the user to generate an actual topology
     * 2. Contain unit structures and port structures to build and bind
     * 3. Allow deriving classes to define a topology
     */
    class CPUTopology{
    public:

        //! \brief Structure to represent a resource unit in device tree
        struct UnitInfo{

            //! ResourceTreeNode name
            std::string name;

            //! ResourceTreeNode parent name
            std::string parent_name;

            //! ResourceTreeNode human-readable name
            std::string human_name;

            //! TreeNode group name required for multiple execution units
            std::string group_name;

            //! TreeNode group id required for multiple execution units
            uint32_t group_id;

            //! Factory required to create this particular resource
            sparta::ResourceFactoryBase* factory;

            //! Flag to tell whether this node should be private to its parent
            bool is_private_subtree;

            /**
             * @brief Constructor for UnitInfo
             */
            UnitInfo(const std::string& name,
                     const std::string& parent_name,
                     const std::string& human_name,
                     const std::string&  group_name,
                     const uint32_t group_id,
                     sparta::ResourceFactoryBase* factory,
                     bool is_private_subtree = false) :
                name{name},
                parent_name{parent_name},
                human_name{human_name},
                group_name{group_name},
                group_id{group_id},
                factory{factory},
                is_private_subtree{is_private_subtree}{}
        };

        //! \brief Structure to represent a port binding between units in device tree
        struct PortConnectionInfo{

            //! Out port name of unit_1
            std::string output_port_name;

            //! In port name of next unit, unit_2
            std::string input_port_name;

            /**
             * @brief Constructor for PortConnectionInfo
             */
            PortConnectionInfo(const std::string& output_port_name,
                               const std::string& input_port_name) :
                output_port_name{output_port_name},
                input_port_name{input_port_name}{}
        };

        /**
         * @brief Constructor for CPUTopology
         */
        CPUTopology() : factories{new CPUFactories()}{}

        /**
         * @brief Set the name for this topoplogy
         */
        auto setName(const std::string& topology) -> void{
            topology_name = topology;
        }

        /**
         * @brief Set the number of cores in this processor
         */
        auto setNumCores(const uint32_t num_of_cores) -> void{
            num_cores = num_of_cores;
        }

        /**
         * @brief Static method to allocate memory for topology
         */
        static auto allocateTopology(const std::string& topology) -> CPUTopology*;

        //! Public members used by CPUFactory to build and bind tree
        uint32_t num_cores = 1;
        std::unique_ptr<CPUFactories> factories;
        std::string topology_name;
        std::vector<UnitInfo> units;
        std::vector<PortConnectionInfo> port_connections;

    }; // class CPUTopology

    class CoreExtensions : public sparta::ExtensionsParamsOnly
    {
    public:
        static constexpr char name[] = "core_extensions";

        using ExecutionTopology      = std::vector<std::vector<std::string>>;
        using ExecutionTopologyParam = sparta::Parameter<ExecutionTopology>;

        CoreExtensions() : sparta::ExtensionsParamsOnly() {}
        virtual ~CoreExtensions() {}

        void postCreate() override {
            sparta::ParameterSet * ps = getParameters();

            //
            // Example of an execution topology:
            //  [["alu", "1"], ["fpu", "1"], ["br",  "1"]]
            //
            //  LSU is its own entity at this time
            //
            execution_topology_.
                reset(new ExecutionTopologyParam("execution_topology", ExecutionTopology(),
                                                 "Topology Post Dispatch -- the execution pipes. "
                                                 "Expect: [[\"<unit_name>\", \"<count>\"]] ", ps));
        }
    private:
        std::unique_ptr<ExecutionTopologyParam> execution_topology_;

    };

    /**
     * @brief CoreTopology_1 topology class
     */
    class CoreTopology_1 : public CPUTopology
    {
    public:
        /**
         * @brief Constructor for CPUTopology
         */
        CoreTopology_1();
    }; // class CoreTopology_1
}  // namespace olympia
