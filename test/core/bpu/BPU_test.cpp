// BPU test

#include "sparta/sparta.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <string>

TEST_INIT
olympia::InstAllocator inst_allocator(2000, 1000);

class BPUSim : public class sparta::app::Simulation
{
    using BPUFactory = sparta::ResourceFactory<olympia::BPU, olympia::BPU::BPUParameterSet>;
    public:
        BPUSim(sparta::Scheduler *sched) : sparta::app::Simulatiuon("BPUSim", sched) 
        input_file_(input_file),
        test_tap_(getRoot(), "info", output_file)
        {}

        virtual ~ICacheSim(){
            getRoot()->enterTeardown();
        }

    private:
        void buildTree_() override {
            auto rtn = getRoot();

            allocators_tn.reset(new olympia::OlympiaAllocators(rtn));

            tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(rtn,
            olympia::MavisUnit::name, sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE, "Mavis Unit",
            &mavis_fact));

            // Create a Source Unit 
            sparta::ResourceTreeNode* src_unit = new sparta::ResourceTreeNode(
            rtn, "src", sparta::TreeNode::GROUP_NAME_NONE, sparta::TreeNode::GROUP_IDX_NONE,
            "Source Unit", &source_fact);

            tns_to_delete_.emplace_back(src_unit);

                    auto* src_params = src_unit->getParameterSet();
            src_params->getParameter("input_file")->setValueFromString(input_file_);

            // Create the device under test
            sparta::ResourceTreeNode* dut =
            new sparta::ResourceTreeNode(rtn, "dut", sparta::TreeNode::GROUP_NAME_NONE,
                                         sparta::TreeNode::GROUP_IDX_NONE, "DUT", &dut_fact);

            tns_to_delete_.emplace_back(dut);

            // Create the Sink unit
            sparta::ResourceTreeNode* sink =
            new sparta::ResourceTreeNode(rtn, "sink", sparta::TreeNode::GROUP_NAME_NONE,
                                         sparta::TreeNode::GROUP_IDX_NONE, "Sink Unit", &sink_fact);

            tns_to_delete_.emplace_back(sink);

            auto* sink_params = sink->getParameterSet();
            sink_params->getParameter("purpose")->setValueFromString("grp");
        }

        void configureTree_() override {};

        void bindTree_() override {
            auto* root_node = getRoot();

// set all ports appropriately
            sparta::bind(root_node->getChildAs<sparta::Port>(), root_node->getChildAs<sparta::Port>());

            sparta::bind(root_node->getChildAs<sparta::Port>(), root_node->getChildAs<sparta::Port>());

            sparta::bind(root_node->getChildAs<sparta::Port>(), root_node->getChildAs<sparta::Port>());

            sparta::bind(root_node->getChildAs<sparta::Port>(), root_node->getChildAs<sparta::Port>());

        }

        std::unique_ptr<olympia::OlympiaAllocators> allocators_tn_;

        olympia::MavisFactory mavis_fact;
        BPUFactory bpu_fact;

        core_test::SrcFactory source_fact;
        core_test::SinkFactory sink_fact;

        std::vector<unique_ptr<sparta::TreeNode>> tns_to_delete_;
        std::vector<sparta::ResourceTreeNode*> exe_units_;

        const std::string input_file_;
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

bool runTest(int argc, char** argv) {
    DEFAULTS.auto_summary_default = "off";
    std::vector<std::string> datafiles;
    std::string input_file;

    sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
    auto & app_opts = cls.getApplicationOptions();

    sparta::app::named_value<vector<string>>("output_file", &datafiles), "Specifies the output file")("input_file",
         sparta::app::named_value<string>("INPUT_FILE", &input_file)->default_value(""),
         "Provide a JSON or STF instruction stream");

    po::positional_options_description & pos_opts = cls.getPositionalOptions();

    pos_opts.add("output_file", -1);

    int err_code = 0;
    if(!cls.parse(argc, argv, err_code)){
        sparta_assert(false, "Command line parsing failed"); // Any errors already printed to cerr
    }
}

int main(int argc, char **argv) {
    if(!runTest(argc, argv)) {
        return 1;
    }

    REPORT_ERROR;
    return (int)ERROR_CODE;
}