
#include "cpu/CPUFactory.hpp"
#include "utils/CoreUtils.hpp"
#include "dispatch/Dispatch.hpp"
#include "decode/MavisUnit.hpp"
#include "OlympiaAllocators.hpp"
#include "OlympiaSim.hpp"
#include "rename/Rename.hpp"

#include "test/core/common/SinkUnit.hpp"
#include "test/core/common/SourceUnit.hpp"
#include "test/core/dispatch/ExecutePipeSinkUnit.hpp"
#include "test/core/rename/ROBSinkUnit.hpp"

#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/resources/Buffer.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/sparta.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <cinttypes>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <vector>

//
// Simple Dispatch Simulator.
//
// SourceUnit -> Dispatch -> 1..* SinkUnits
//
class DispatchSim : public sparta::app::Simulation
{
  public:
    DispatchSim(sparta::Scheduler* sched, const std::string & mavis_isa_files,
                const std::string & mavis_uarch_files, const std::string & output_file,
                const std::string & input_file, const bool vector_enabled) :
        sparta::app::Simulation("DispatchSim", sched),
        input_file_(input_file),
        test_tap_(getRoot(), "info", output_file)
    {
    }

    ~DispatchSim() { getRoot()->enterTeardown(); }

    void runRaw(uint64_t run_time) override final
    {
        (void)run_time; // ignored

        sparta::app::Simulation::runRaw(run_time);
    }

  private:
    void buildTree_() override
    {
        auto rtn = getRoot();
        // Cerate the common Allocators
        allocators_tn_.reset(new olympia::OlympiaAllocators(rtn));

        sparta::ResourceTreeNode* disp = nullptr;

        // Create a Mavis Unit
        tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(
            rtn, olympia::MavisUnit::name, sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE, "Mavis Unit", &mavis_fact));

        sparta::ResourceTreeNode* decode_unit = nullptr;
        tns_to_delete_.emplace_back(
            decode_unit = new sparta::ResourceTreeNode(
                rtn, olympia::Decode::name, sparta::TreeNode::GROUP_NAME_NONE,
                sparta::TreeNode::GROUP_IDX_NONE, "Decode Unit", &source_fact));
        decode_unit->getParameterSet()->getParameter("input_file")->setValueFromString(input_file_);
        decode_unit->getParameterSet()->getParameter("test_type")->setValueFromString("multiple");
        // Create Dispatch
        tns_to_delete_.emplace_back(
            disp = new sparta::ResourceTreeNode(
                rtn, olympia::Dispatch::name, sparta::TreeNode::GROUP_NAME_NONE,
                sparta::TreeNode::GROUP_IDX_NONE, "Test Dispatch", &dispatch_fact));

        // Create Execute -> ExecutePipes and IssueQueues
        sparta::ResourceTreeNode* execute_unit = new sparta::ResourceTreeNode(
            rtn, olympia::Execute::name, sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE, "Test Rename", &execute_factory_);
        tns_to_delete_.emplace_back(execute_unit);
        // Create Rename
        sparta::ResourceTreeNode* rename_unit = new sparta::ResourceTreeNode(
            rtn, olympia::Rename::name, sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE, "Test Rename", &rename_fact);
        tns_to_delete_.emplace_back(rename_unit);
        // Create SinkUnit that represents the ROB
        sparta::ResourceTreeNode* rob = nullptr;
        tns_to_delete_.emplace_back(
            rob = new sparta::ResourceTreeNode(rtn, "rob", sparta::TreeNode::GROUP_NAME_NONE,
                                               sparta::TreeNode::GROUP_IDX_NONE, "Sink Unit",
                                               &rob_sink_fact));
        auto* rob_params = rob->getParameterSet();
        // Set the "ROB" to accept a group of instructions
        rob_params->getParameter("purpose")->setValueFromString("single");

        // Must add the CoreExtensions factory so the simulator knows
        // how to interpret the config file extension parameter.
        // Otherwise, the framework will add any "unnamed" extensions
        // as strings.
        rtn->addExtensionFactory(olympia::CoreExtensions::name,
                                 [&]() -> sparta::TreeNode::ExtensionsBase*
                                 { return new olympia::CoreExtensions(); });

        sparta::ResourceTreeNode* exe_unit = nullptr;
        //

        // Create the LSU sink separately
        tns_to_delete_.emplace_back(exe_unit = new sparta::ResourceTreeNode(
                                        rtn, "lsu", sparta::TreeNode::GROUP_NAME_NONE,
                                        sparta::TreeNode::GROUP_IDX_NONE, "Sink Unit", &sink_fact));
        auto* exe_params = exe_unit->getParameterSet();
        exe_params->getParameter("purpose")->setValueFromString("single");
    }

    void configureTree_() override {}

    void bindTree_() override
    {
        auto* root_node = getRoot();
        // Bind the "ROB" (simple SinkUnit that accepts instruction
        // groups) to Dispatch
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_reorder_buffer_write"),
                     root_node->getChildAs<sparta::Port>("rob.ports.in_sink_inst_grp"));
        sparta::bind(
            root_node->getChildAs<sparta::Port>("dispatch.ports.in_reorder_buffer_credits"),
            root_node->getChildAs<sparta::Port>("rob.ports.out_sink_credits"));

        // Bind the Rename ports
        sparta::bind(root_node->getChildAs<sparta::Port>("rename.ports.out_dispatch_queue_write"),
                     root_node->getChildAs<sparta::Port>("dispatch.ports.in_dispatch_queue_write"));

        sparta::bind(
            root_node->getChildAs<sparta::Port>("rename.ports.in_dispatch_queue_credits"),
            root_node->getChildAs<sparta::Port>("dispatch.ports.out_dispatch_queue_credits"));

        sparta::bind(root_node->getChildAs<sparta::Port>("decode.ports.in_credits"),
                     root_node->getChildAs<sparta::Port>("rename.ports.out_uop_queue_credits"));
        sparta::bind(root_node->getChildAs<sparta::Port>("rename.ports.in_uop_queue_append"),
                     root_node->getChildAs<sparta::Port>("decode.ports.out_instgrp_write"));

        const std::string dispatch_ports = "dispatch.ports";
        const std::string flushmanager_ports = "flushmanager.ports";
        auto issue_queue_to_pipe_map =
            olympia::coreutils::getPipeTopology(root_node, "issue_queue_to_pipe_map");
        auto bind_ports = [root_node](const std::string & left, const std::string & right)
        {
            sparta::bind(root_node->getChildAs<sparta::Port>(left),
                         root_node->getChildAs<sparta::Port>(right));
        };
        for (size_t i = 0; i < issue_queue_to_pipe_map.size(); ++i)
        {
            auto iq = issue_queue_to_pipe_map[i];
            std::string unit_name = "iq" + std::to_string(i);
            const std::string exe_credits_out =
                "execute." + unit_name + ".ports.out_scheduler_credits";
            const std::string disp_credits_in = dispatch_ports + ".in_" + unit_name + "_credits";
            bind_ports(exe_credits_out, disp_credits_in);

            // Bind instruction transfer
            const std::string exe_inst_in = "execute." + unit_name + ".ports.in_execute_write";
            const std::string disp_inst_out = dispatch_ports + ".out_" + unit_name + "_write";
            bind_ports(exe_inst_in, disp_inst_out);
            // in_execute_pipe
            const std::string exe_pipe_in = "execute." + unit_name + ".ports.in_execute_pipe";

            auto pipe_target_start = stoi(iq[0]);
            auto pipe_target_end = stoi(iq[0]);
            if (iq.size() > 1)
            {
                pipe_target_end = stoi(iq[1]);
            }
            pipe_target_end++;
            for (int pipe_idx = pipe_target_start; pipe_idx < pipe_target_end; ++pipe_idx)
            {
                const std::string exe_pipe_out =
                    "execute.exe" + std::to_string(pipe_idx) + ".ports.out_execute_pipe";
                bind_ports(exe_pipe_in, exe_pipe_out);
            }
        }
        // Bind the "LSU" SinkUnit to Dispatch
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_lsu_write"),
                     root_node->getChildAs<sparta::Port>("lsu.ports.in_sink_inst"));

        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.in_lsu_credits"),
                     root_node->getChildAs<sparta::Port>("lsu.ports.out_sink_credits"));
    }

    // Allocators.  Last thing to delete
    std::unique_ptr<olympia::OlympiaAllocators> allocators_tn_;
    sparta::ResourceFactory<olympia::Decode, olympia::Decode::DecodeParameterSet> decode_fact;
    olympia::DispatchFactory dispatch_fact;
    olympia::IssueQueueFactory issue_queue_fact_;
    olympia::MavisFactory mavis_fact;
    olympia::RenameFactory rename_fact;
    core_test::SourceUnitFactory source_fact;
    core_test::SinkUnitFactory sink_fact;
    core_test::ExecutePipeSinkUnitFactory execute_pipe_sink_fact_;
    rename_test::ROBSinkUnitFactory rob_sink_fact;
    olympia::ExecutePipeFactory execute_pipe_fact_;
    olympia::ExecuteFactory execute_factory_;

    std::vector<std::unique_ptr<sparta::TreeNode>> tns_to_delete_;
    std::vector<sparta::ResourceTreeNode*> exe_units_;

    const std::string input_file_;
    sparta::log::Tap test_tap_;
};
