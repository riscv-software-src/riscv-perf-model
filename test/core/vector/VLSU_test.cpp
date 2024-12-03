
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

    void test_num_insts_completed(const uint32_t expected_val)
    {
        EXPECT_EQUAL(vlsu_->lsu_insts_completed_.get(), expected_val);
    }

    void test_num_mem_reqs(const uint32_t expected_val)
    {
        EXPECT_EQUAL(vlsu_->memory_requests_generated_.get(), expected_val);
    }

private:
    olympia::VLSU * vlsu_;
};

void runTests(int argc, char **argv) {
    DEFAULTS.auto_summary_default = "off";
    std::string input_file;

    sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
    auto &app_opts = cls.getApplicationOptions();
    app_opts.add_options()
        ("input-file",
            sparta::app::named_value<std::string>("INPUT_FILE", &input_file)->default_value(""),
            "Provide a JSON instruction stream",
            "Provide a JSON file with instructions to run through Execute");

    int err_code = 0;
    if (!cls.parse(argc, argv, err_code))
    {
        sparta_assert(false,
            "Command line parsing failed"); // Any errors already printed to cerr
    }

    sparta::Scheduler scheduler;
    uint32_t num_cores = 1;
    uint64_t ilimit = 0;
    bool show_factories = false;
    OlympiaSim sim("simple",
                   scheduler,
                   num_cores,
                   input_file,
                   ilimit,
                   show_factories);
    sparta::RootTreeNode *root_node = sim.getRoot();
    cls.populateSimulation(&sim);

    olympia::VLSU *my_vlsu = \
            root_node->getChild("cpu.core0.vlsu")->getResourceAs<olympia::VLSU*>();
    olympia::VLSUTester vlsu_tester {my_vlsu};

    if (input_file.find("vlsu_load.json") != std::string::npos)
    {
        // Test VLSU
        cls.runSimulator(&sim);
        vlsu_tester.test_num_insts_completed(2);
        // First load: vle64.v with LMUL = 4 (64 mem reqs)
        // Second load: vle8.v with LMUL = 1 (128 reqs)
        vlsu_tester.test_num_mem_reqs(64 + 128);
    }
    else if (input_file.find("vlsu_store.json") != std::string::npos)
    {
        // Test VLSU
        vlsu_tester.test_num_insts_completed(2);
        vlsu_tester.test_num_mem_reqs(128);
    }
    else
    {
        sparta_assert(false, "Invalid input file: " << input_file);
    }
}

int main(int argc, char **argv) {
    runTests(argc, argv);

    REPORT_ERROR;
    return (int)ERROR_CODE;
}
