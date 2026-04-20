// <CoreExtension.h> -*- C++ -*-

#pragma once

#include <vector>
#include <string>
#include <memory>

#include "sparta/simulation/TreeNodeExtensions.hpp"
#include "sparta/simulation/ParameterSet.hpp"

namespace olympia
{

    //
    // \class CoreExtensions
    // \brief Common extensions for a specific core
    //
    // Similar to Parameters, Extensions allow the modeler to provide
    // common "preferences" to any node (and it's children).  For
    // example, the topology of the execution units: the number of
    // ALUs.  Both Dispatch and Execute (as well as testers) need to
    // know this information.
    //
    //
    class CoreExtensions : public sparta::ExtensionsParamsOnly
    {
      public:
        static constexpr char name[] = "core_extensions";

        using ExecutionTopology = std::vector<std::vector<std::string>>;
        using ExecutionTopologyParam = sparta::Parameter<ExecutionTopology>;

        using PipeTopology = std::vector<std::vector<std::string>>;
        using PipeTopologyParam = sparta::Parameter<PipeTopology>;

        using IssueQueueTopology = std::vector<std::vector<std::string>>;
        using IssueQueueTopologyParam = sparta::Parameter<IssueQueueTopology>;

        CoreExtensions() : sparta::ExtensionsParamsOnly() {}

        virtual ~CoreExtensions() {}

        void postCreate() override
        {
            sparta::ParameterSet* ps = getParameters();

            //
            // Example of an execution topology:
            //  [["alu", "1"], ["fpu", "1"], ["br",  "1"]]
            //
            //  LSU is its own entity at this time
            //
            execution_topology_.reset(
                new ExecutionTopologyParam("execution_topology", ExecutionTopology(),
                                           "Topology Post Dispatch -- the execution pipes. "
                                           "Expect: [[\"<unit_name>\", \"<count>\"]] ",
                                           ps));
            pipelines_.reset(new PipeTopologyParam("pipelines", PipeTopology(),
                                                   "Topology Mapping"
                                                   "Mapping of Pipe Targets to execution unit",
                                                   ps));
            issue_queue_to_pipe_map_.reset(
                new IssueQueueTopologyParam("issue_queue_to_pipe_map", IssueQueueTopology(),
                                            "Issue Queue Topology"
                                            "Defines Issue Queue to Execution Unit Mapping",
                                            ps));
            exe_pipe_rename_.reset(new IssueQueueTopologyParam("exe_pipe_rename",
                                                              IssueQueueTopology(),
                                                              "Rename for ExecutionPipes"
                                                              "Defines alias for ExecutionPipe",
                                                              ps));
            issue_queue_rename_.reset(new IssueQueueTopologyParam("issue_queue_rename",
                                                                 IssueQueueTopology(),
                                                                 "Rename for IssueQueues"
                                                                 "Defines alias for IssueQueue",
                                                                 ps));
        }

      private:
        std::unique_ptr<ExecutionTopologyParam> execution_topology_;
        std::unique_ptr<PipeTopologyParam> pipelines_;
        std::unique_ptr<IssueQueueTopologyParam> issue_queue_to_pipe_map_;
        std::unique_ptr<PipeTopologyParam> exe_pipe_rename_;
        std::unique_ptr<PipeTopologyParam> issue_queue_rename_;
    };
}
