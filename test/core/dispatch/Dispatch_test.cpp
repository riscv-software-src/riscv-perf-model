
#include "Dispatch.hpp"
#include "MavisUnit.hpp"

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

class DispatchSim : public sparta::app::Simulation
{
public:

    DispatchSim(sparta::Scheduler * sched,
                const std::string & mavis_isa_files,
                const std::string & mavis_uarch_files,
                const std::string & output_file,
                const std::string & json_input_file,
                const bool vector_enabled) :
        sparta::app::Simulation("DispatchSim", sched),
        test_tap_(getRoot(), "info", output_file)
    {
        // if(!json_input_file.empty()) {
        //     inst_generator_.reset(new olympia::JSONInstGenerator(mavis_facade_, json_input_file));
        // }
    }

    ~DispatchSim() {
        getRoot()->enterTeardown();
    }

    olympia::Dispatch * getDispatchBlock() {
        return dispatch_;
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

        // Create a Source Unit
        tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(rtn,
                                                                 core_test::SourceUnit::name,
                                                                 sparta::TreeNode::GROUP_NAME_NONE,
                                                                 sparta::TreeNode::GROUP_IDX_NONE,
                                                                 "Source Unit",
                                                                 &source_fact));

        // Create Dispatch
        tns_to_delete_.emplace_back(disp = new sparta::ResourceTreeNode(rtn,
                                                                        olympia::Dispatch::name,
                                                                        sparta::TreeNode::GROUP_NAME_NONE,
                                                                        sparta::TreeNode::GROUP_IDX_NONE,
                                                                        "Test Dispatch",
                                                                        &dispatch_fact));

        // Create SinkUnit, one per execution unit
        tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(rtn,
                                                                 core_test::SinkUnit::name,
                                                                 sparta::TreeNode::GROUP_NAME_NONE,
                                                                 sparta::TreeNode::GROUP_IDX_NONE,
                                                                 "Sink Unit",
                                                                 &sink_fact));

        rtn->addExtensionFactory("pipe_assignments",
                                 [&]() -> sparta::TreeNode::ExtensionsBase * {return new olympia::ExecutionPipeAssignments();});

        rtn->addExtensionFactory("issue_queue_assignments",
                                 [&]() -> sparta::TreeNode::ExtensionsBase * {return new olympia::IssueQueueAssignments();});

        rtn->addExtensionFactory("vector",
                                 [&]() -> sparta::TreeNode::ExtensionsBase * {return new olympia::VectorExtension();});

        rtn->addExtensionFactory("pipeline_assignments",
                                 [&]() -> sparta::TreeNode::ExtensionsBase * {return new olympia::PipelineAssignments();});

        rtn->addExtensionFactory("simulation_configuration",
                                 [&]() -> sparta::TreeNode::ExtensionsBase * {return new olympia::CoreExtensions();});
        auto issueq_assignments = rtn->getExtension("issue_queue_assignments");
        sparta_assert(nullptr != issueq_assignments,
                      "Issue queue assignments are null.  Are you missing the issue queue extensions?");
        auto issueq_ext_params = issueq_assignments->getParameters();
        num_vex_units_ = issueq_ext_params->getParameterValueAs<uint32_t>("num_vex_issue_queues");

        // Create the three issue queues and the rob (as an IssueBlock)
        for(std::string unit : supported_scalar_units)
        {
            std::ostringstream name;
            name << unit << "_issue";

            tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(rtn, name.str(),
                                                                     sparta::TreeNode::GROUP_NAME_NONE,
                                                                     sparta::TreeNode::GROUP_IDX_NONE,
                                                                     name.str(), &issue_fact));
        }

        sparta::ResourceTreeNode * rob;
        tns_to_delete_.emplace_back(rob = new sparta::ResourceTreeNode(rtn,
                                                                       "retire",
                                                                       sparta::TreeNode::GROUP_NAME_NONE,
                                                                       sparta::TreeNode::GROUP_IDX_NONE,
                                                                       "ROB Issue",
                                                                       &issue_fact));
        auto * rob_params = rob->getParameterSet();
        rob_params->getParameter("purpose")->setValueFromString("rob");
        rob_params->getParameter("dispWidth")->setValueFromString
            (disp->getParameterSet()->getParameter("main_dispWidth")->getValueAsString());

    }

    void configureTree_()  override { }

    void bindTree_()  override
    {
        auto * root_node = getRoot();

        // Create the Mavis Unit
        // Bind the source unit to dispatch
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.in_dispatch_append0"),
                     root_node->getChildAs<sparta::Port>("source_unit.ports.out_dispatch_write0"));
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.in_dispatch_append1"),
                     root_node->getChildAs<sparta::Port>("source_unit.ports.out_dispatch_write1"));
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_dispatch_credits0"),
                     root_node->getChildAs<sparta::Port>("source_unit.ports.in_dispatch_credits0"));
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_dispatch_credits1"),
                     root_node->getChildAs<sparta::Port>("source_unit.ports.in_dispatch_credits1"));

        // Bind the issue blocks to dispatch
        for(std::string unit : supported_scalar_units)
        {
            sparta::bind(root_node->getChildAs<sparta::Port>("dispatch."+unit+".ports.out_issue"),
                         root_node->getChildAs<sparta::Port>(unit+"_issue.ports.in_issue_inst"));

            sparta::bind(root_node->getChildAs<sparta::Port>("dispatch."+unit+".ports.in_issue_credits"),
                         root_node->getChildAs<sparta::Port>(unit+"_issue.ports.out_issue_credits"));
        }

        // TODO: Don't hook up these pipes for dual pipeline test (ROB append should happen in Decode)
        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.out_rob_issue"),
                     root_node->getChildAs<sparta::Port>("retire.ports.in_issue_inst"));

        sparta::bind(root_node->getChildAs<sparta::Port>("dispatch.ports.in_rob_credits"),
                     root_node->getChildAs<sparta::Port>("retire.ports.out_rob_credits"));

        dispatch_    = root_node->getChild("dispatch")->getResourceAs<olympia::Dispatch>();
        source_unit_ = root_node->getChild("source_unit")->getResourceAs<core_test::SourceUnit>();
        source_unit_->setInstGenerator(inst_generator_.get());

    }

    olympia::DispatchFactory     dispatch_fact;
    olympia::MavisFactoy         mavis_fact;
    core_test::SourceUnitFactory source_fact;
    core_test::SinkUnitFactory   sink_fact;

    std::vector<std::unique_ptr<sparta::TreeNode>> tns_to_delete_;

    olympia::Dispatch     * dispatch_ = nullptr;
    core_test::SourceUnit * source_unit_ = nullptr;
    core_test::SinkUnit   * sink_unit_   = nullptr;

    std::unique_ptr<olympia::InstGenerator> inst_generator_;
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
    std::string json_input_file;
    bool enable_vector;

    sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
    auto & app_opts = cls.getApplicationOptions();
    app_opts.add_options()
        ("output_file",
         sparta::app::named_value<std::vector<std::string>>("output_file", &datafiles),
         "Specifies the output file")
        ("json-file,j",
         sparta::app::named_value<std::string>("JSON_INPUT", &json_input_file)->default_value(""),
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
    DispatchSim sim(&sched, "mavis_isa_files", "arch/isa_json", datafiles[0], json_input_file, enable_vector);

    cls.populateSimulation(&sim);
    cls.runSimulator(&sim);

    EXPECT_FILES_EQUAL(datafiles[0], datafiles[0] + ".EXPECTED");
}

int main(int argc, char **argv)
{
    runTest(argc, argv);

    REPORT_ERROR;
    return ERROR_CODE;
}
