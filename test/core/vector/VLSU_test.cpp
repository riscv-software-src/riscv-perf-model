
#include "CPUFactory.hpp"
#include "CoreUtils.hpp"
#include "Dispatch.hpp"
#include "MavisUnit.hpp"
#include "OlympiaAllocators.hpp"
#include "OlympiaSim.hpp"
#include "IssueQueue.hpp"
#include "test/core/dispatch/Dispatch_test.hpp"

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

////////////////////////////////////////////////////////////////////////////////
// Set up the Mavis decoder globally for the testing
olympia::InstAllocator inst_allocator(2000, 1000);

const char USAGE[] = "Usage:\n"
                     "    \n"
                     "\n";

sparta::app::DefaultValues DEFAULTS;
class olympia::VLSUTester
{
public:
    VLSUTester(olympia::VLSU * vlsu) :
        vlsu_(vlsu)
    {}

    void test_mem_request_count(const uint32_t expected_val)
    {
        EXPECT_TRUE(vlsu_->inst_queue_.read(0)->getCurrVLSUIters() == expected_val);
    }


private:
    olympia::VLSU * vlsu_;
    
};
void runTests(int argc, char **argv) {
    DEFAULTS.auto_summary_default = "off";
    std::vector<std::string> datafiles;
    std::string input_file;
    bool enable_vector;

    sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
    auto &app_opts = cls.getApplicationOptions();
    app_opts.add_options()("output_file",
                                                 sparta::app::named_value<std::vector<std::string>>(
                                                         "output_file", &datafiles),
                                                 "Specifies the output file")(
            "input-file",
            sparta::app::named_value<std::string>("INPUT_FILE", &input_file)
                    ->default_value(""),
            "Provide a JSON instruction stream",
            "Provide a JSON file with instructions to run through Execute")(
            "enable_vector",
            sparta::app::named_value<bool>("enable_vector", &enable_vector)
                    ->default_value(false),
            "Enable the experimental vector pipelines");

    po::positional_options_description &pos_opts = cls.getPositionalOptions();
    pos_opts.add("output_file", -1); // example, look for the <data file> at the end

    int err_code = 0;
    if (!cls.parse(argc, argv, err_code)) {
        sparta_assert(false,
            "Command line parsing failed"); // Any errors already printed to cerr
    }

    sparta_assert(false == datafiles.empty(),
        "Need an output file as the last argument of the test");

    uint64_t ilimit = 0;
    uint32_t num_cores = 1;
    bool show_factories = false;
    sparta::Scheduler scheduler;
    OlympiaSim sim("simple", scheduler,
                                 num_cores, // cores
                                 input_file, ilimit, show_factories);
    sparta::RootTreeNode *root_node = sim.getRoot();
    cls.populateSimulation(&sim);
    olympia::VLSU *my_vlsu = \
            root_node->getChild("cpu.core0.vlsu")->getResourceAs<olympia::VLSU*>();
    olympia::VLSUTester vlsu_tester {my_vlsu};

    if (input_file.find("vlsu_load_multiple.json") != std::string::npos) {
        // Test VLSU
        cls.runSimulator(&sim, 57);
        vlsu_tester.test_mem_request_count(13);
    }
    else if (input_file.find("vlsu_store.json") != std::string::npos) {
        // Test VLSU
        cls.runSimulator(&sim, 61);
        vlsu_tester.test_mem_request_count(9);
    }
    else{
        cls.runSimulator(&sim);
    }
}

int main(int argc, char **argv) {
    runTests(argc, argv);

    REPORT_ERROR;
    return (int)ERROR_CODE;
}
