
#include "Dispatch.hpp"
#include "MavisUnit.hpp"
#include "CoreUtils.hpp"
#include "Rename.hpp"
#include "ExecutePipe.hpp"
#include "LSU.hpp"
#include "sim/OlympiaSim.hpp"
#include "OlympiaAllocators.hpp"

#include "test/core/common/SourceUnit.hpp"
#include "test/core/common/SinkUnit.hpp"
#include "test/core/rename/ROBSinkUnit.hpp"

#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/resources/Buffer.hpp"
#include "sparta/sparta.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"

#include <memory>
#include <vector>
#include <cinttypes>
#include <sstream>
#include <initializer_list>

TEST_INIT

////////////////////////////////////////////////////////////////////////////////
// Set up the Mavis decoder globally for the testing
olympia::InstAllocator inst_allocator(2000, 1000);


class olympia::RenameTester
{
public:
    void test_clearing_rename_structures(olympia::Rename & rename){
        // after all instructions have retired, we should have:
        // num_rename_registers - 32 registers = freelist size
        // because we initialize the first 32 registers
        if (rename.reference_counter_[0].size() == 34){
            EXPECT_TRUE(rename.freelist_[0].size() == 2);
            // in the case of only two free PRFs, they should NOT be equal to each other
            EXPECT_TRUE(rename.freelist_[0].front() != rename.freelist_[0].back());
        }
        else{
            EXPECT_TRUE(rename.freelist_[0].size() == 96);
        }
        // we're only expecting one reference
        EXPECT_TRUE(rename.reference_counter_[0][1] == 1);
        EXPECT_TRUE(rename.reference_counter_[0][2] == 1);

    }
    void test_one_instruction(olympia::Rename & rename){
        // process only one instruction, check that freelist and map_tables are allocated correctly
        if (rename.reference_counter_[0].size() == 34){
            EXPECT_TRUE(rename.freelist_[0].size() == 1);
        }
        else{
            EXPECT_TRUE(rename.freelist_[0].size() == 95);
        }
        // map table entry is valid, as it's been allocated

        // reference counters should now be 2 because the first instruction is:
        // ADD x3 x1 x2 and both x1 -> prf1 and x2 -> prf2
        EXPECT_TRUE(rename.reference_counter_[0][1] == 2);
        EXPECT_TRUE(rename.reference_counter_[0][2] == 2);
    }
    void test_multiple_instructions(olympia::Rename & rename){
        // first two instructions are RAW
        // so the second instruction should increase reference count
        EXPECT_TRUE(rename.reference_counter_[0][2] == 2);
    }
    void test_startup_rename_structures(olympia::Rename & rename){
        // before starting, we should have:
        // num_rename_registers - 32 registers = freelist size
        // because we initialize the first 32 registers
        if (rename.reference_counter_[0].size() == 34){
            EXPECT_TRUE(rename.freelist_[0].size() == 2);
        }
        else{
            EXPECT_TRUE(rename.freelist_[0].size() == 96);
        }
        // we're only expecting a value of 1 for registers x0 -> x31 because we initialize them
        EXPECT_TRUE(rename.reference_counter_[0][1] == 1);
        EXPECT_TRUE(rename.reference_counter_[0][2] == 1);
        EXPECT_TRUE(rename.reference_counter_[0][30] == 1);
        EXPECT_TRUE(rename.reference_counter_[0][31] == 1);
        //
        EXPECT_TRUE(rename.reference_counter_[0][33] == 0);
        EXPECT_TRUE(rename.reference_counter_[0][34] == 0);

    }
    void test_float(olympia::Rename & rename){
        // ensure the correct register file is used
        EXPECT_TRUE(rename.freelist_[1].size() == 94);
        EXPECT_TRUE(rename.freelist_[0].size() == 96);
    }
};

class olympia::ExecutePipeTester
{
public:
    void test_dependent_integer_first_instruction(olympia::ExecutePipe & executepipe){
        // testing RAW dependency for ExecutePipe
        // only alu0 should have an issued instruction
        // so alu0's total_insts_issued should be 1
        EXPECT_TRUE(executepipe.total_insts_issued_ == 1);
    }
    void test_dependent_integer_second_instruction(olympia::ExecutePipe & executepipe){
        // testing RAW dependency for ExecutePipe
        // only alu0 should have an issued instruction
        // alu1 shouldn't, hence this test is checking for alu1's issued inst count is 0
        EXPECT_TRUE(executepipe.total_insts_issued_ == 0);
    }
};

class olympia::LSUTester
{
public:
    void test_dependent_lsu_instruction(olympia::LSU & lsu){
        // testing RAW dependency for LSU
        // we have an ADD instruction before we destination register 3
        // and then a subsequent STORE instruction to register 3
        // we can't STORE until the add instruction runs, so we test
        // while the ADD instruction is running, the STORE instruction should NOT issue
        EXPECT_TRUE(lsu.lsu_insts_issued_ == 0);
    }

    void clear_entries(olympia::LSU &lsu){
        auto iter = lsu.ldst_inst_queue_.begin();
        while(iter != lsu.ldst_inst_queue_.end()){
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

    RenameSim(sparta::Scheduler * sched,
              const std::string & mavis_isa_files,
              const std::string & mavis_uarch_files,
              const std::string & output_file,
              const std::string & input_file) :
        sparta::app::Simulation("RenameSim", sched),
        input_file_(input_file),
        test_tap_(getRoot(), "info", output_file)
    {
    }

    ~RenameSim() {
        getRoot()->enterTeardown();
    }

    void runRaw(uint64_t run_time) override final
    {
        (void) run_time; // ignored

        sparta::app::Simulation::runRaw(run_time);
    }

private:

    void buildTree_()  override
    {
        auto rtn = getRoot();

        // Cerate the common Allocators
        allocators_tn_.reset(new olympia::OlympiaAllocators(rtn));

        sparta::ResourceTreeNode * disp = nullptr;

        // Create a Mavis Unit
        tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(rtn,
                                                                 olympia::MavisUnit::name,
                                                                 sparta::TreeNode::GROUP_NAME_NONE,
                                                                 sparta::TreeNode::GROUP_IDX_NONE,
                                                                 "Mavis Unit",
                                                                 &mavis_fact));

        // Create a Source Unit
        tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(rtn,
                                                                 core_test::SourceUnit::name,
                                                                 sparta::TreeNode::GROUP_NAME_NONE,
                                                                 sparta::TreeNode::GROUP_IDX_NONE,
                                                                 "Source Unit",
                                                                 &source_fact));
        sparta::ResourceTreeNode * decode_unit = nullptr;
        tns_to_delete_.emplace_back(decode_unit = new sparta::ResourceTreeNode(rtn,
                                                                               olympia::Decode::name,
                                                                               sparta::TreeNode::GROUP_NAME_NONE,
                                                                               sparta::TreeNode::GROUP_IDX_NONE,
                                                                               "Decode Unit",
                                                                               &source_fact));
        decode_unit->getParameterSet()->getParameter("input_file")->setValueFromString(input_file_);
        // Create Dispatch
        tns_to_delete_.emplace_back(disp = new sparta::ResourceTreeNode(rtn,
                                                                        olympia::Dispatch::name,
                                                                        sparta::TreeNode::GROUP_NAME_NONE,
                                                                        sparta::TreeNode::GROUP_IDX_NONE,
                                                                        "Dispatch Unit",
                                                                        &dispatch_fact));

        // Create Rename
        sparta::ResourceTreeNode * rename_unit = new sparta::ResourceTreeNode(rtn,
                                                                              olympia::Rename::name,
                                                                              sparta::TreeNode::GROUP_NAME_NONE,
                                                                              sparta::TreeNode::GROUP_IDX_NONE,
                                                                              "Test Rename",
                                                                              &rename_fact);
        tns_to_delete_.emplace_back(rename_unit);

        // Create SinkUnit that represents the ROB
        sparta::ResourceTreeNode * rob  = nullptr;
        tns_to_delete_.emplace_back(rob = new sparta::ResourceTreeNode(rtn,
                                                                       "rob",
                                                                       sparta::TreeNode::GROUP_NAME_NONE,
                                                                       sparta::TreeNode::GROUP_IDX_NONE,
                                                                       "Sink Unit",
                                                                       &rob_sink_fact));
        auto * rob_params = rob->getParameterSet();
        // Set the "ROB" to accept a group of instructions
        rob_params->getParameter("purpose")->setValueFromString("grp");

        // Must add the CoreExtensions factory so the simulator knows
        // how to interpret the config file extension parameter.
        // Otherwise, the framework will add any "unnamed" extensions
        // as strings.
        rtn->addExtensionFactory(olympia::CoreExtensions::name,
                                 [&]() -> sparta::TreeNode::ExtensionsBase * {return new olympia::CoreExtensions();});

        sparta::ResourceTreeNode * exe_unit = nullptr;
        //
        // Set up the execution blocks (sans LSU)
        //
        auto execution_topology = olympia::coreutils::getExecutionTopology(rtn);
        for (auto exe_unit_pair : execution_topology)
        {
            const auto tgt_name   = exe_unit_pair[0];
            const auto unit_count = exe_unit_pair[1];
            const auto exe_idx    = (unsigned int) std::stoul(unit_count);
            sparta_assert(exe_idx > 0, "Expected more than 0 units! " << tgt_name);
            for(uint32_t unit_num = 0; unit_num < exe_idx; ++unit_num)
            {
                const std::string unit_name = tgt_name + std::to_string(unit_num);

                // Create N units (alu0, alu1, etc)
                tns_to_delete_.emplace_back(exe_unit = new sparta::ResourceTreeNode(rtn,
                                                                                    unit_name,
                                                                                    tgt_name,
                                                                                    unit_num,
                                                                                    unit_name + " Exe pipe",
                                                                                    &rob_sink_fact));
                auto exe_params = exe_unit->getParameterSet();
                // Set the "exe unit" to accept a single instruction
                exe_params->getParameter("purpose")->setValueFromString("single");
                exe_units_.emplace_back(exe_unit);
            }
        }

        // Create the LSU sink separately
        tns_to_delete_.emplace_back(exe_unit = new sparta::ResourceTreeNode(rtn,
                                                                            "lsu",
                                                                            sparta::TreeNode::GROUP_NAME_NONE,
                                                                            sparta::TreeNode::GROUP_IDX_NONE,
                                                                            "Sink Unit",
                                                                            &rob_sink_fact));
        auto * exe_params = exe_unit->getParameterSet();
        exe_params->getParameter("purpose")->setValueFromString("single");
    }

    void configureTree_()  override { }

    void bindTree_()  override
    {
        auto * root_node = getRoot();

        // Bind the "ROB" (simple SinkUnit that accepts instruction
        // groups) to Dispatch
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_reorder_buffer_write"),
                     root_node->getChildAs<sparta::Port>("rob.ports.in_sink_inst_grp"));
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_reorder_buffer_write"),
                     root_node->getChildAs<sparta::Port>("rob.ports.in_sink_retire_inst_grp"));

        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.in_reorder_buffer_credits"),
                     root_node->getChildAs<sparta::Port>("rob.ports.out_sink_credits"));

        // Bind the Rename ports
        sparta::bind(root_node->getChildAs<sparta::Port>("rename.ports.out_dispatch_queue_write"),
                     root_node->getChildAs<sparta::Port>("dispatch.ports.in_dispatch_queue_write"));

        sparta::bind(root_node->getChildAs<sparta::Port>("rename.ports.in_dispatch_queue_credits"),
                     root_node->getChildAs<sparta::Port>("dispatch.ports.out_dispatch_queue_credits"));

        sparta::bind(root_node->getChildAs<sparta::Port>("decode.ports.in_credits"),
                     root_node->getChildAs<sparta::Port>("rename.ports.out_uop_queue_credits"));
        sparta::bind(root_node->getChildAs<sparta::Port>("rename.ports.in_uop_queue_append"),
                     root_node->getChildAs<sparta::Port>("decode.ports.out_instgrp_write"));

        // Bind the "exe" SinkUnit blocks to dispatch
        for(auto exe_unit : exe_units_)
        {
            const std::string unit_name = exe_unit->getName();
            sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_"+unit_name+"_write"),
                         root_node->getChildAs<sparta::Port>(unit_name + ".ports.in_sink_inst"));
            sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.in_"+unit_name+"_credits"),
                         root_node->getChildAs<sparta::Port>(unit_name+".ports.out_sink_credits"));
            sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_"+unit_name+"_write"),
                         root_node->getChildAs<sparta::Port>(unit_name + ".ports.in_sink_retire_inst"));
            sparta::bind(root_node->getChildAs<sparta::Port>("rename.ports.in_rename_retire_ack"),
                         root_node->getChildAs<sparta::Port>(unit_name+".ports.out_rob_retire_ack"));
        }

        // Bind the "LSU" SinkUnit to Dispatch
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_lsu_write"),
                     root_node->getChildAs<sparta::Port>("lsu.ports.in_sink_inst"));
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_lsu_write"),
                     root_node->getChildAs<sparta::Port>("lsu.ports.in_sink_retire_inst"));
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.in_lsu_credits"),
                     root_node->getChildAs<sparta::Port>("lsu.ports.out_sink_credits"));
        sparta::bind(root_node->getChildAs<sparta::Port>("rename.ports.in_rename_retire_ack"),
                     root_node->getChildAs<sparta::Port>("lsu.ports.out_rob_retire_ack"));
    }
    // Allocators.  Last thing to delete
    std::unique_ptr<olympia::OlympiaAllocators> allocators_tn_;

    sparta::ResourceFactory<olympia::Decode,
                            olympia::Decode::DecodeParameterSet> decode_fact;
    olympia::DispatchFactory        dispatch_fact;
    olympia::RenameFactory          rename_fact;
    olympia::MavisFactoy            mavis_fact;
    core_test::SourceUnitFactory    source_fact;
    core_test::SinkUnitFactory      sink_fact;
    rename_test::ROBSinkUnitFactory   rob_sink_fact;


    std::vector<std::unique_ptr<sparta::TreeNode>> tns_to_delete_;
    std::vector<sparta::ResourceTreeNode*>         exe_units_;

    const std::string input_file_;
    sparta::log::Tap test_tap_;
};

const char USAGE[] =
    "Usage:\n"
    "    \n"
    "\n";

sparta::app::DefaultValues DEFAULTS;

// The main tester of Rename.  The test is encapsulated in the
// parameter test_type of the Source unit.
void runTest(int argc, char **argv)
{
    DEFAULTS.auto_summary_default = "off";
    std::vector<std::string> datafiles;
    std::string input_file;

    sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
    auto & app_opts = cls.getApplicationOptions();
    app_opts.add_options()
        ("output_file",
         sparta::app::named_value<std::vector<std::string>>("output_file", &datafiles),
         "Specifies the output file")
        ("input-file",
         sparta::app::named_value<std::string>("INPUT_FILE", &input_file)->default_value(""),
         "Provide a JSON instruction stream",
         "Provide a JSON file with instructions to run through Execute");

    po::positional_options_description& pos_opts = cls.getPositionalOptions();
    pos_opts.add("output_file", -1); // example, look for the <data file> at the end

    int err_code = 0;
    if(!cls.parse(argc, argv, err_code)){
        sparta_assert(false, "Command line parsing failed"); // Any errors already printed to cerr
    }

    sparta_assert(false == datafiles.empty(), "Need an output file as the last argument of the test");

    sparta::Scheduler scheduler;
    uint64_t ilimit = 0;
    uint32_t num_cores = 1;
    bool show_factories = false;
    OlympiaSim sim("simple",
                   scheduler,
                   num_cores, // cores
                   input_file,
                   ilimit,
                   show_factories);

    if(input_file == "raw_integer.json"){
        cls.populateSimulation(&sim);
        sparta::RootTreeNode* root_node = sim.getRoot();

        olympia::ExecutePipe* my_executepipe = root_node->getChild("cpu.core0.execute.alu0")->getResourceAs<olympia::ExecutePipe*>();
        olympia::ExecutePipe* my_executepipe1 = root_node->getChild("cpu.core0.execute.alu1")->getResourceAs<olympia::ExecutePipe*>();
        olympia::ExecutePipeTester executepipe_tester;
        cls.runSimulator(&sim, 7);
        executepipe_tester.test_dependent_integer_first_instruction(*my_executepipe);
        executepipe_tester.test_dependent_integer_second_instruction(*my_executepipe1);
    }
    else if(input_file == "i2f.json"){
        cls.populateSimulation(&sim);
        sparta::RootTreeNode* root_node = sim.getRoot();
        olympia::Rename* my_rename = root_node->getChild("cpu.core0.rename")->getResourceAs<olympia::Rename*>();

        olympia::RenameTester rename_tester;
        cls.runSimulator(&sim, 4);
        rename_tester.test_float(*my_rename);
    }
    else if(input_file == "raw_int_lsu.json"){
        // testing RAW dependency for address operand
        cls.populateSimulation(&sim);
        sparta::RootTreeNode* root_node = sim.getRoot();

        olympia::LSU* my_lsu = root_node->getChild("cpu.core0.lsu")->getResourceAs<olympia::LSU*>();
        olympia::LSUTester lsu_tester;
        olympia::ExecutePipe* my_executepipe = root_node->getChild("cpu.core0.execute.alu0")->getResourceAs<olympia::ExecutePipe*>();
        olympia::ExecutePipeTester executepipe_tester;
        cls.runSimulator(&sim, 6);
        executepipe_tester.test_dependent_integer_first_instruction(*my_executepipe);
        lsu_tester.test_dependent_lsu_instruction(*my_lsu);
        lsu_tester.clear_entries(*my_lsu);
    }
    else if(input_file == "raw_float_lsu.json"){
        // testing RAW dependency for data operand
        cls.populateSimulation(&sim);
        sparta::RootTreeNode* root_node = sim.getRoot();

        olympia::LSU* my_lsu = root_node->getChild("cpu.core0.lsu")->getResourceAs<olympia::LSU*>();
        olympia::LSUTester lsu_tester;
        olympia::ExecutePipe* my_executepipe = root_node->getChild("cpu.core0.execute.fpu0")->getResourceAs<olympia::ExecutePipe*>();
        olympia::ExecutePipeTester executepipe_tester;
        cls.runSimulator(&sim, 6);
        executepipe_tester.test_dependent_integer_first_instruction(*my_executepipe);
        lsu_tester.test_dependent_lsu_instruction(*my_lsu);
        lsu_tester.clear_entries(*my_lsu);
    }
    else{
        sparta::Scheduler sched;
        RenameSim rename_sim(&sched, "mavis_isa_files", "arch/isa_json", datafiles[0], input_file);

        cls.populateSimulation(&rename_sim);

        sparta::RootTreeNode* root_node =  rename_sim.getRoot();
        olympia::Rename* my_rename = root_node->getChild("rename")->getResourceAs<olympia::Rename*>();
        olympia::RenameTester rename_tester;
        rename_tester.test_startup_rename_structures(*my_rename);
        cls.runSimulator(&rename_sim, 2);
        rename_tester.test_one_instruction(*my_rename);

        cls.runSimulator(&rename_sim, 3);
        rename_tester.test_multiple_instructions(*my_rename);

        cls.runSimulator(&rename_sim);
        rename_tester.test_clearing_rename_structures(*my_rename);

        EXPECT_FILES_EQUAL(datafiles[0], "expected_output/" + datafiles[0] + ".EXPECTED");
    }
}

int main(int argc, char **argv)
{
    runTest(argc, argv);

    REPORT_ERROR;
    return (int)ERROR_CODE;
}
