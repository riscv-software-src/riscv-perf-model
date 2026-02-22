
#include "utils/CoreUtils.hpp"
#include "dispatch/Dispatch.hpp"
#include "execute/ExecutePipe.hpp"
#include "lsu/LSU.hpp"
#include "decode/MavisUnit.hpp"
#include "sim/OlympiaAllocators.hpp"
#include "rename/Rename.hpp"
#include "decode/Decode.hpp"
#include "execute/IssueQueue.hpp"
#include "rob/ROB.hpp"
#include "execute/Execute.hpp"
#include "sim/OlympiaSim.hpp"

#include "test/core/common/SinkUnit.hpp"
#include "test/core/common/SourceUnit.hpp"
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

TEST_INIT

class olympia::RenameTester
{
public:
    void test_clearing_rename_structures(const olympia::Rename & rename)
    {
        const auto & rf_components = rename.regfile_components_[0];
        // after all instructions have retired, we should have:
        // num_rename_registers - 31 registers = freelist size
        // because we initialize the first 31 registers (x1-x31) for integer
        if (rf_components.reference_counter.size() == 34)
        {
            EXPECT_EQUAL(rf_components.freelist.size(), 2);
            // in the case of only two free PRFs, they should NOT be equal to each
            // other
            EXPECT_TRUE(rf_components.freelist.front() != rf_components.freelist.back());
        }
        else
        {
            EXPECT_EQUAL(rf_components.freelist.size(), 96);
        }
        // we're only expecting one reference
        EXPECT_EQUAL(rf_components.reference_counter[1].cnt, 1);
        EXPECT_EQUAL(rf_components.reference_counter[2].cnt, 1);
    }

    void test_clearing_rename_structures_amoadd(const olympia::Rename & rename)
    {
        const auto & rf_components = rename.regfile_components_[0];
        // after all instructions have retired, we should have:
        // num_rename_registers - 32 registers = freelist size
        // because we initialize the first 32 registers
        if (rf_components.reference_counter.size() == 34)
        {
            EXPECT_EQUAL(rf_components.freelist.size(), 2);
            // in the case of only two free PRFs, they should NOT be equal to each
            // other
            EXPECT_TRUE(rf_components.freelist.front() != rf_components.freelist.back());
        }
        else
        {
            EXPECT_EQUAL(rf_components.freelist.size(), 96);
        }
        // The bottom 3 references should be cleared (amoadd returns)
        EXPECT_EQUAL(rf_components.reference_counter[0].cnt, 0);
        EXPECT_EQUAL(rf_components.reference_counter[1].cnt, 0);
        EXPECT_EQUAL(rf_components.reference_counter[2].cnt, 0);
        EXPECT_EQUAL(rf_components.reference_counter[3].cnt, 1);
    }

    void test_one_instruction(const olympia::Rename & rename)
    {
        const auto & rf_components = rename.regfile_components_[0];
        // process only one instruction, check that freelist and map_tables are
        // allocated correctly
        if (rf_components.reference_counter.size() == 34)
        {
            EXPECT_EQUAL(rf_components.freelist.size(), 1);
        }
        else
        {
            EXPECT_EQUAL(rf_components.freelist.size(), 95);
        }
        // map table entry is valid, as it's been allocated

        // reference counters should now be 2 because the first instruction is:
        // ADD x3 x1 x2 and both x1 -> prf1 and x2 -> prf2
        EXPECT_EQUAL(rf_components.reference_counter[1].cnt, 1);
        EXPECT_EQUAL(rf_components.reference_counter[2].cnt, 1);
    }

    void test_multiple_instructions(const olympia::Rename & rename)
    {
        const auto & rf_components = rename.regfile_components_[0];
        // first two instructions are RAW
        // so the second instruction should increase reference count
        EXPECT_EQUAL(rf_components.reference_counter[2].cnt, 1);
    }

    void test_startup_rename_structures(const olympia::Rename & rename)
    {
        const auto & rf_components = rename.regfile_components_[0];
        // before starting, we should have:
        // num_rename_registers - 32 registers = freelist size
        // because we initialize the first 32 registers
        if (rf_components.reference_counter.size() == 34)
        {
            EXPECT_EQUAL(rf_components.freelist.size(), 2);
        }
        else
        {
            EXPECT_EQUAL(rf_components.freelist.size(), 96);
        }
        // we're only expecting a value of 1 for registers x0 -> x31 because we
        // initialize them
        EXPECT_EQUAL(rf_components.reference_counter[1].cnt, 1);
        EXPECT_EQUAL(rf_components.reference_counter[2].cnt, 1);
        EXPECT_EQUAL(rf_components.reference_counter[30].cnt, 1);
        EXPECT_EQUAL(rf_components.reference_counter[31].cnt, 1);
        // x0 for RF_INTEGER should be not assigned because x0 for RF_INTEGER doesn't use physical
        // registerfile since it's hardcoded to 0, no need to rename, so we get an extra free
        // register back. Test to make sure this is actually modeled
        EXPECT_EQUAL(rf_components.reference_counter[0].cnt, 0);
        EXPECT_EQUAL(rf_components.reference_counter[32].cnt, 0);
        EXPECT_EQUAL(rf_components.reference_counter[33].cnt, 0);
    }

    void test_float(const olympia::Rename & rename)
    {
        const auto & rf_components_0 = rename.regfile_components_[0];
        const auto & rf_components_1 = rename.regfile_components_[1];

        // ensure the correct register file is used
        EXPECT_EQUAL(rf_components_1.freelist.size(), 94);
        EXPECT_EQUAL(rf_components_0.freelist.size(), 96);
    }
};

class olympia::IssueQueueTester
{
public:
    void test_dependent_integer_first_instruction(olympia::IssueQueue & issuequeue)
    {
        // testing RAW dependency for ExecutePipe
        // only alu0 should have an issued instruction
        // so alu0's total_insts_issued should be 1
        EXPECT_EQUAL(issuequeue.total_insts_issued_, 1ull);
    }

    void test_dependent_integer_second_instruction(olympia::IssueQueue & issuequeue)
    {
        // testing RAW dependency for ExecutePipe
        // only alu0 should have an issued instruction
        // alu1 shouldn't, hence this test is checking for alu1's issued inst count
        // is 0
        EXPECT_EQUAL(issuequeue.total_insts_issued_, 0ull);
    }
};

class olympia::LSUTester
{
public:
    void test_dependent_lsu_instruction(olympia::LSU & lsu)
    {
        // testing RAW dependency for LSU
        // we have an ADD instruction before we destination register 3
        // and then a subsequent STORE instruction to register 3
        // we can't STORE until the add instruction runs, so we test
        // while the ADD instruction is running, the STORE instruction should NOT
        // issue
        EXPECT_EQUAL(lsu.lsu_insts_issued_, 0ull);
    }

    void clear_entries(olympia::LSU & lsu)
    {
        auto iter = lsu.ldst_inst_queue_.begin();
        while (iter != lsu.ldst_inst_queue_.end())
        {
            auto x(iter++);
            lsu.ldst_inst_queue_.erase(x);
        }
    }
};

//
// Simple Rename Simulator.
//
// SourceUnit -> Decode -> Rename -> Dispatch -> 1..* SinkUnits
//
class RenameSim : public sparta::app::Simulation
{
public:
    RenameSim(sparta::Scheduler* sched, const std::string & mavis_isa_files,
              const std::string & mavis_uarch_files, const std::string & output_file,
              const std::string & input_file) :
        sparta::app::Simulation("RenameSim", sched),
        input_file_(input_file),
        test_tap_(getRoot(), "info", output_file)
    {
    }

    ~RenameSim() { getRoot()->enterTeardown(); }

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

        // Create a Source Unit -- this would represent Rename
        // sparta::ResourceTreeNode * src_unit  = nullptr;
        // tns_to_delete_.emplace_back(src_unit = new sparta::ResourceTreeNode(rtn,
        //                                                                     core_test::SourceUnit::name,
        //                                                                     sparta::TreeNode::GROUP_NAME_NONE,
        //                                                                     sparta::TreeNode::GROUP_IDX_NONE,
        //                                                                     "Source
        //                                                                     Unit",
        //                                                                     &source_fact));
        // src_unit->getParameterSet()->getParameter("input_file")->setValueFromString(input_file_);
        sparta::ResourceTreeNode* decode_unit = nullptr;
        tns_to_delete_.emplace_back(
            decode_unit = new sparta::ResourceTreeNode(
                rtn, olympia::Decode::name, sparta::TreeNode::GROUP_NAME_NONE,
                sparta::TreeNode::GROUP_IDX_NONE, "Decode Unit", &source_fact));
        decode_unit->getParameterSet()->getParameter("input_file")->setValueFromString(input_file_);
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
        tns_to_delete_.emplace_back(rob = new sparta::ResourceTreeNode(
                                        rtn, "rob", sparta::TreeNode::GROUP_NAME_NONE,
                                        sparta::TreeNode::GROUP_IDX_NONE, "ROB Unit", &rob_fact_));
        // auto * rob_params = rob->getParameterSet();
        //  Set the "ROB" to accept a group of instructions
        // rob_params->getParameter("purpose")->setValueFromString("single");

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
                     root_node->getChildAs<sparta::Port>("rob.ports.in_reorder_buffer_write"));

        sparta::bind(
            root_node->getChildAs<sparta::Port>("dispatch.ports.in_reorder_buffer_credits"),
            root_node->getChildAs<sparta::Port>("rob.ports.out_reorder_buffer_credits"));

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

        sparta::bind(root_node->getChildAs<sparta::Port>("rename.ports.in_rename_retire_ack"),
                     root_node->getChildAs<sparta::Port>("rob.ports.out_rob_retire_ack_rename"));

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
    rename_test::ROBSinkUnitFactory rob_sink_fact;
    olympia::ExecutePipeFactory execute_pipe_fact_;
    olympia::ExecuteFactory execute_factory_;
    sparta::ResourceFactory<olympia::ROB, olympia::ROB::ROBParameterSet> rob_fact_;

    std::vector<std::unique_ptr<sparta::TreeNode>> tns_to_delete_;
    std::vector<sparta::ResourceTreeNode*> exe_units_;
    std::vector<std::unique_ptr<sparta::ResourceTreeNode>> issue_queues_;

    const std::string input_file_;
    sparta::log::Tap test_tap_;
};

const char USAGE[] = "Usage:\n"
    "    \n"
    "\n";

sparta::app::DefaultValues DEFAULTS;

// The main tester of Rename.  The test is encapsulated in the
// parameter test_type of the Source unit.
void runTest(int argc, char** argv)
{
    DEFAULTS.auto_summary_default = "off";
    std::vector<std::string> datafiles;
    std::string input_file;

    sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
    auto & app_opts = cls.getApplicationOptions();
    app_opts.add_options()(
        "output_file",
        sparta::app::named_value<std::vector<std::string>>("output_file", &datafiles),
        "Specifies the output file")(
            "input-file",
            sparta::app::named_value<std::string>("INPUT_FILE", &input_file)->default_value(""),
            "Provide a JSON instruction stream",
            "Provide a JSON file with instructions to run through Execute");

    po::positional_options_description & pos_opts = cls.getPositionalOptions();
    pos_opts.add("output_file",
                 -1); // example, look for the <data file> at the end

    int err_code = 0;
    if (!cls.parse(argc, argv, err_code))
    {
        sparta_assert(false,
                      "Command line parsing failed"); // Any errors already printed to cerr
    }

    sparta_assert(false == datafiles.empty(),
                  "Need an output file as the last argument of the test");

    sparta::Scheduler scheduler;
    uint64_t ilimit = 0;
    uint32_t num_cores = 1;
    bool show_factories = false;
    OlympiaSim sim(scheduler,
                   num_cores, // cores
                   input_file, ilimit, show_factories);

    if (input_file == "raw_integer.json")
    {
        cls.populateSimulation(&sim);
        sparta::RootTreeNode* root_node = sim.getRoot();

        olympia::IssueQueue* my_issuequeue =
            root_node->getChild("cpu.core0.execute.iq0")->getResourceAs<olympia::IssueQueue*>();
        olympia::IssueQueue* my_issuequeue1 =
            root_node->getChild("cpu.core0.execute.iq1")->getResourceAs<olympia::IssueQueue*>();
        olympia::IssueQueueTester issuequeue_tester;
        cls.runSimulator(&sim, 8);
        issuequeue_tester.test_dependent_integer_first_instruction(*my_issuequeue);
        issuequeue_tester.test_dependent_integer_second_instruction(*my_issuequeue1);
    }
    else if (input_file == "i2f.json")
    {
        cls.populateSimulation(&sim);
        sparta::RootTreeNode* root_node = sim.getRoot();
        olympia::Rename* my_rename =
            root_node->getChild("cpu.core0.rename")->getResourceAs<olympia::Rename*>();

        olympia::RenameTester rename_tester;
        // Must stop the simulation before it retires the i2f instructions,
        // otherwise the register would have been moved back into the freelist
        cls.runSimulator(&sim, 8);
        rename_tester.test_float(*my_rename);
    }
    else if (input_file == "raw_int_lsu.json")
    {
        // testing RAW dependency for address operand
        cls.populateSimulation(&sim);
        sparta::RootTreeNode* root_node = sim.getRoot();

        olympia::LSU* my_lsu = root_node->getChild("cpu.core0.lsu")->getResourceAs<olympia::LSU*>();
        olympia::LSUTester lsu_tester;
        olympia::IssueQueue* my_issuequeue =
            root_node->getChild("cpu.core0.execute.iq0")->getResourceAs<olympia::IssueQueue*>();
        olympia::IssueQueueTester issuequeue_tester;
        cls.runSimulator(&sim, 8);
        issuequeue_tester.test_dependent_integer_first_instruction(*my_issuequeue);
        lsu_tester.test_dependent_lsu_instruction(*my_lsu);
        lsu_tester.clear_entries(*my_lsu);
    }
    else if (input_file == "raw_float_lsu.json")
    {
        // testing RAW dependency for data operand
        cls.populateSimulation(&sim);
        sparta::RootTreeNode* root_node = sim.getRoot();

        olympia::LSU* my_lsu = root_node->getChild("cpu.core0.lsu")->getResourceAs<olympia::LSU*>();
        olympia::LSUTester lsu_tester;
        // assuming iq1 is floating point issue queue
        olympia::IssueQueue* my_issuequeue =
            root_node->getChild("cpu.core0.execute.iq1")->getResourceAs<olympia::IssueQueue*>();
        olympia::IssueQueueTester issuequeue_tester;
        cls.runSimulator(&sim, 8);
        issuequeue_tester.test_dependent_integer_first_instruction(*my_issuequeue);
        lsu_tester.test_dependent_lsu_instruction(*my_lsu);
        lsu_tester.clear_entries(*my_lsu);
    }
    else if (input_file.find("amoadd.json") != std::string::npos)
    {
        sparta::Scheduler sched;

        cls.populateSimulation(&sim);

        sparta::RootTreeNode* root_node = sim.getRoot();
        olympia::Rename* my_rename =
            root_node->getChild("cpu.core0.rename")->getResourceAs<olympia::Rename*>();
        olympia::RenameTester rename_tester;
        cls.runSimulator(&sim);
        rename_tester.test_clearing_rename_structures_amoadd(*my_rename);
    }
    else if (input_file.find("rename_multiple_instructions_full.json") != std::string::npos)
    {
        sparta::RootTreeNode* root_node = sim.getRoot();
        cls.populateSimulation(&sim);
        cls.runSimulator(&sim);
        olympia::Rename* my_rename =
            root_node->getChild("cpu.core0.rename")->getResourceAs<olympia::Rename*>();
        olympia::RenameTester rename_tester;
        rename_tester.test_clearing_rename_structures(*my_rename);
    }
    else
    {
        sparta::Scheduler sched;
        RenameSim rename_sim(&sched, "mavis_isa_files", "arch/isa_json", datafiles[0], input_file);

        cls.populateSimulation(&rename_sim);

        sparta::RootTreeNode* root_node = rename_sim.getRoot();
        olympia::Rename* my_rename =
            root_node->getChild("rename")->getResourceAs<olympia::Rename*>();
        olympia::RenameTester rename_tester;
        rename_tester.test_startup_rename_structures(*my_rename);
        cls.runSimulator(&rename_sim, 2);
        rename_tester.test_one_instruction(*my_rename);

        cls.runSimulator(&rename_sim, 3);
        rename_tester.test_multiple_instructions(*my_rename);

        EXPECT_FILES_EQUAL(datafiles[0], "expected_output/" + datafiles[0] + ".EXPECTED");
    }
}

int main(int argc, char** argv)
{
    runTest(argc, argv);

    REPORT_ERROR;
    return (int)ERROR_CODE;
}
