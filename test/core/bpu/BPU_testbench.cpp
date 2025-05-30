#include "../../../core/fetch/BPU.hpp"
#include "../../../core/FTQ.hpp"
#include "decode/MavisUnit.hpp"
#include "OlympiaAllocators.hpp"
#include "BPUSink.hpp"
#include "BPUSource.hpp"

#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <iostream>
#include <vector>

#include <fstream>

using namespace std;

TEST_INIT
olympia::InstAllocator inst_allocator(2000, 1000);

// ----------------------------------------------------------------------
// Testbench reference - example starting point for unit benches
// ----------------------------------------------------------------------
class Simulator : public sparta::app::Simulation
{
    using BPUFactory = sparta::ResourceFactory<olympia::BranchPredictor::BPU,
                                               olympia::BranchPredictor::BPU::BPUParameterSet>;

    using FTQFactory = sparta::ResourceFactory<olympia::FTQ,
                                               olympia::FTQ::FTQParameterSet>;

  public:
    Simulator(sparta::Scheduler* sched, const string & mavis_isa_files,
              const string & mavis_uarch_files, const string & output_file,
              const string & input_file) :
        sparta::app::Simulation("Simulator", sched),
        input_file_(input_file),
        test_tap_(getRoot(), "info", output_file)
    {
    }

    virtual ~Simulator() { 
        std::cout << "Simulator destructor\n";
        getRoot()->enterTeardown(); 
    }

    void runRaw(uint64_t run_time) override final
    {
        (void)run_time; // ignored

        sparta::app::Simulation::runRaw(run_time);
    }

  private:
    void buildTree_() override
    {
        auto rtn = getRoot();

        // Create the common allocators
        allocators_tn_.reset(new olympia::OlympiaAllocators(rtn));

        // Create a Mavis Unit
        tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(
            rtn, olympia::MavisUnit::name, sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE, "Mavis Unit", &mavis_fact));

        // Create a Source Unit
        sparta::ResourceTreeNode* src_unit = new sparta::ResourceTreeNode(
            rtn, "src", sparta::TreeNode::GROUP_NAME_NONE, sparta::TreeNode::GROUP_IDX_NONE,
            "Source Unit", &source_fact);

        tns_to_delete_.emplace_back(src_unit);

        //auto* src_params = src_unit->getParameterSet();
        //src_params->getParameter("input_file")->setValueFromString(input_file_);

        // Create the device under test

        // Create BPU
        sparta::ResourceTreeNode* bpu =
            new sparta::ResourceTreeNode(rtn, "bpu", sparta::TreeNode::GROUP_NAME_NONE,
                                         sparta::TreeNode::GROUP_IDX_NONE, "BPU", &bpu_fact);

        tns_to_delete_.emplace_back(bpu);

        // Create FTQ
        sparta::ResourceTreeNode* ftq = 
            new sparta::ResourceTreeNode(rtn, "ftq", sparta::TreeNode::GROUP_NAME_NONE,
                                         sparta::TreeNode::GROUP_IDX_NONE, "FTQ", &ftq_fact);

        tns_to_delete_.emplace_back(ftq);

        // Create the Sink unit
        sparta::ResourceTreeNode* sink =
            new sparta::ResourceTreeNode(rtn, "sink", sparta::TreeNode::GROUP_NAME_NONE,
                                         sparta::TreeNode::GROUP_IDX_NONE, "Sink Unit", &sink_fact);

        tns_to_delete_.emplace_back(sink);

        //auto* sink_params = sink->getParameterSet();
        //sink_params->getParameter("purpose")->setValueFromString("grp");
    }

    void configureTree_() override {}

    void bindTree_() override
    {
        auto* root_node = getRoot();

        // See the README.md for A/B/Cx/Dx
        //
        // Credit is transferred from BPU to Source
        sparta::bind(
            root_node->getChildAs<sparta::Port>("bpu.ports.out_fetch_credits"),
            root_node->getChildAs<sparta::Port>("src.ports.in_bpu_credits"));

        // Movement of PredictionRequest from Source to BPU
        sparta::bind(
            root_node->getChildAs<sparta::Port>("bpu.ports.in_fetch_prediction_request"),
            root_node->getChildAs<sparta::Port>("src.ports.out_bpu_prediction_request"));

        // Credits is transferred from Sink to FTQ
        sparta::bind(
            root_node->getChildAs<sparta::Port>("ftq.ports.in_fetch_credits"),
            root_node->getChildAs<sparta::Port>("sink.ports.out_ftq_credits"));

        // Movement of PredictionOutput from FTQ to Sink
        sparta::bind(
            root_node->getChildAs<sparta::Port>("ftq.ports.out_fetch_prediction_output"),
            root_node->getChildAs<sparta::Port>("sink.ports.in_ftq_prediction_output"));

        // Binding BPU and FTQ
        sparta::bind(
            root_node->getChildAs<sparta::Port>("ftq.ports.out_bpu_update_input"),
            root_node->getChildAs<sparta::Port>("bpu.ports.in_ftq_update_input"));

        sparta::bind(
            root_node->getChildAs<sparta::Port>("ftq.ports.out_bpu_credits"),
            root_node->getChildAs<sparta::Port>("bpu.ports.in_ftq_credits"));
            
        sparta::bind(
            root_node->getChildAs<sparta::Port>("ftq.ports.in_bpu_first_prediction_output"),
            root_node->getChildAs<sparta::Port>("bpu.ports.out_ftq_first_prediction_output"));
            
        sparta::bind(
            root_node->getChildAs<sparta::Port>("ftq.ports.in_bpu_second_prediction_output"),
            root_node->getChildAs<sparta::Port>("bpu.ports.out_ftq_second_prediction_output"));
        
    }

    // Allocators.  Last thing to delete
    std::unique_ptr<olympia::OlympiaAllocators> allocators_tn_;

    olympia::MavisFactory mavis_fact;
    BPUFactory bpu_fact;
    FTQFactory ftq_fact;

    bpu_test::SrcFactory source_fact;
    bpu_test::SinkFactory sink_fact;

    vector<unique_ptr<sparta::TreeNode>> tns_to_delete_;
    vector<sparta::ResourceTreeNode*> exe_units_;

    const string input_file_;
    sparta::log::Tap test_tap_;
};

const char USAGE[] = "Usage:\n\n"
                     "Testbench options \n"
                     "    [ --input_file ]   : json or stf input file\n"
                     "    [ --output_file ]  : output file for results checking\n"
                     " \n"
                     "Commonly used options \n"
                     "    [-i insts] [-r RUNTIME] [--show-tree] [--show-dag]\n"
                     "    [-p PATTERN VAL] [-c FILENAME]\n"
                     "    [-l PATTERN CATEGORY DEST]\n"
                     "    [-h,--help] <workload [stf trace or JSON]>\n"
                     "\n";

sparta::app::DefaultValues DEFAULTS;

// ----------------------------------------------------------------
// ----------------------------------------------------------------
bool runTest(int argc, char** argv)
{
    DEFAULTS.auto_summary_default = "off";
    vector<string> datafiles;
    string input_file;

    sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
    auto & app_opts = cls.getApplicationOptions();

    app_opts.add_options()("output_file",
                           sparta::app::named_value<vector<string>>("output_file", &datafiles),
                           "Specifies the output file")

        ("input_file",
         sparta::app::named_value<string>("INPUT_FILE", &input_file)->default_value(""),
         "Provide a JSON or STF instruction stream");

    po::positional_options_description & pos_opts = cls.getPositionalOptions();
    // example, look for the <data file> at the end
    pos_opts.add("output_file", -1);

    int err_code = 0;

    if (!cls.parse(argc, argv, err_code))
    {
        sparta_assert(false, "Command line parsing failed");
    }

    auto & vm = cls.getVariablesMap();
    if (vm.count("tbhelp") != 0)
    {
        cout << USAGE << endl;
        return false;
    }

    sparta_assert(false == datafiles.empty(),
                  "Need an output file as the last argument of the test");

    sparta::Scheduler sched;
    Simulator sim(&sched, "mavis_isa_files", "arch/isa_json", datafiles[0], input_file);

    if (input_file.length() == 0)
    {
        sparta::log::MessageSource il(sim.getRoot(), "info", "Info Messages");
        il << "No input file specified, exiting gracefully, output not checked";
        return true; // not an error
    }
    cls.populateSimulation(&sim);
    cls.runSimulator(&sim);

    std::cout << "file name: " << datafiles[0] << std::endl;
    ifstream Myfile(datafiles[0]);

    std::string str;
    while(getline(Myfile, str)) {
        std::cout << str << std::endl;
    }
    Myfile.close();

    EXPECT_FILES_EQUAL(datafiles[0], "expected_output/" + datafiles[0] + ".EXPECTED");
    return true;
}

int main(int argc, char** argv)
{
    if (!runTest(argc, argv))
        return 1;

    REPORT_ERROR;
    return (int)ERROR_CODE;
}
