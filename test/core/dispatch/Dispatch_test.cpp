
#include "Dispatch.hpp"
#include "MavisUnit.hpp"
#include "CoreUtils.hpp"

#include "test/core/common/SourceUnit.hpp"
#include "test/core/common/SinkUnit.hpp"

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

//
// Simple Dispatch Simulator.
//
// SourceUnit -> Dispatch -> 1..* SinkUnits
//
class DispatchSim : public sparta::app::Simulation
{
public:

    DispatchSim(sparta::Scheduler * sched,
                const std::string & mavis_isa_files,
                const std::string & mavis_uarch_files,
                const std::string & output_file,
                const std::string & input_file,
                const bool vector_enabled) :
        sparta::app::Simulation("DispatchSim", sched),
        input_file_(input_file),
        test_tap_(getRoot(), "info", output_file)
    {
    }

    ~DispatchSim() {
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

        sparta::ResourceTreeNode * disp = nullptr;

        // Create a Mavis Unit
        tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(rtn,
                                                                 olympia::MavisUnit::name,
                                                                 sparta::TreeNode::GROUP_NAME_NONE,
                                                                 sparta::TreeNode::GROUP_IDX_NONE,
                                                                 "Mavis Unit",
                                                                 &mavis_fact));

        // Create a Source Unit -- this would represent Rename
        sparta::ResourceTreeNode * src_unit  = nullptr;
        tns_to_delete_.emplace_back(src_unit = new sparta::ResourceTreeNode(rtn,
                                                                            core_test::SourceUnit::name,
                                                                            sparta::TreeNode::GROUP_NAME_NONE,
                                                                            sparta::TreeNode::GROUP_IDX_NONE,
                                                                            "Source Unit",
                                                                            &source_fact));
        src_unit->getParameterSet()->getParameter("input_file")->setValueFromString(input_file_);

        // Create Dispatch
        tns_to_delete_.emplace_back(disp = new sparta::ResourceTreeNode(rtn,
                                                                        olympia::Dispatch::name,
                                                                        sparta::TreeNode::GROUP_NAME_NONE,
                                                                        sparta::TreeNode::GROUP_IDX_NONE,
                                                                        "Test Dispatch",
                                                                        &dispatch_fact));

        // Create SinkUnit that represents the ROB
        sparta::ResourceTreeNode * rob  = nullptr;
        tns_to_delete_.emplace_back(rob = new sparta::ResourceTreeNode(rtn,
                                                                       "rob",
                                                                       sparta::TreeNode::GROUP_NAME_NONE,
                                                                       sparta::TreeNode::GROUP_IDX_NONE,
                                                                       "Sink Unit",
                                                                       &sink_fact));
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
                                                                                    &sink_fact));
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
                                                                            &sink_fact));
        auto * exe_params = exe_unit->getParameterSet();
        exe_params->getParameter("purpose")->setValueFromString("single");
    }

    void configureTree_()  override { }

    void bindTree_()  override
    {
        auto * root_node = getRoot();

        // Bind the source unit to dispatch
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.in_dispatch_queue_write"),
                     root_node->getChildAs<sparta::Port>("source_unit.ports.out_instgrp_write"));
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_dispatch_queue_credits"),
                     root_node->getChildAs<sparta::Port>("source_unit.ports.in_credits"));

        // Bind the "ROB" (simple SinkUnit that accepts instruction
        // groups) to Dispatch
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_reorder_buffer_write"),
                     root_node->getChildAs<sparta::Port>("rob.ports.in_sink_inst_grp"));
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.in_reorder_buffer_credits"),
                     root_node->getChildAs<sparta::Port>("rob.ports.out_sink_credits"));

        // Bind the "exe" SinkUnit blocks to dispatch
        for(auto exe_unit : exe_units_)
        {
            const std::string unit_name = exe_unit->getName();
            sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_"+unit_name+"_write"),
                         root_node->getChildAs<sparta::Port>(unit_name + ".ports.in_sink_inst"));

            sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.in_"+unit_name+"_credits"),
                         root_node->getChildAs<sparta::Port>(unit_name+".ports.out_sink_credits"));
        }

        // Bind the "LSU" SinkUnit to Dispatch
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_lsu_write"),
                     root_node->getChildAs<sparta::Port>("lsu.ports.in_sink_inst"));

        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.in_lsu_credits"),
                     root_node->getChildAs<sparta::Port>("lsu.ports.out_sink_credits"));
    }

    olympia::DispatchFactory     dispatch_fact;
    olympia::MavisFactoy         mavis_fact;
    core_test::SourceUnitFactory source_fact;
    core_test::SinkUnitFactory   sink_fact;

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

// The main tester of Dispatch.  The test is encapsulated in the
// parameter test_type of the Source unit.
void runTest(int argc, char **argv)
{
    DEFAULTS.auto_summary_default = "off";
    std::vector<std::string> datafiles;
    std::string input_file;
    bool enable_vector;

    sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
    auto & app_opts = cls.getApplicationOptions();
    app_opts.add_options()
        ("output_file",
         sparta::app::named_value<std::vector<std::string>>("output_file", &datafiles),
         "Specifies the output file")
        ("input-file",
         sparta::app::named_value<std::string>("INPUT_FILE", &input_file)->default_value(""),
         "Provide a JSON instruction stream",
         "Provide a JSON file with instructions to run through Execute")
        ("enable_vector",
         sparta::app::named_value<bool>("enable_vector", &enable_vector)->default_value(false),
         "Enable the experimental vector pipelines");

    po::positional_options_description& pos_opts = cls.getPositionalOptions();
    pos_opts.add("output_file", -1); // example, look for the <data file> at the end

    int err_code = 0;
    if(!cls.parse(argc, argv, err_code)){
        sparta_assert(false, "Command line parsing failed"); // Any errors already printed to cerr
    }

    sparta_assert(false == datafiles.empty(), "Need an output file as the last argument of the test");

    sparta::Scheduler sched;
    DispatchSim sim(&sched, "mavis_isa_files", "arch/isa_json", datafiles[0], input_file, enable_vector);

    cls.populateSimulation(&sim);
    cls.runSimulator(&sim);

    EXPECT_FILES_EQUAL(datafiles[0], "expected_output/" + datafiles[0] + ".EXPECTED");
}

int main(int argc, char **argv)
{
    runTest(argc, argv);

    REPORT_ERROR;
    return (int)ERROR_CODE;
}
