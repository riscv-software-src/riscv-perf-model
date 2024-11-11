#include <iostream>

#include "CPUFactory.hpp"
#include "CoreUtils.hpp"
#include "Dispatch.hpp"
#include "Inst.hpp"
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
    VLSUTester(olympia::VLSU* vlsu) : vlsu_(vlsu) {}

    void test_mem_request_count(const uint32_t expected_val)
    {
        std::cout << "qsz: " << vlsu_->inst_queue_.size() << std::endl;

        // THIS ASSERT FAILS
        EXPECT_TRUE(vlsu_->inst_queue_.size() > 0);
        auto inst_ptr = vlsu_->inst_queue_.read(0)->getInstPtr();

        std::cout << "EXP: " << expected_val << std::endl;
        std::cout << "VAL TOTAL: " << inst_ptr->getVectorMemConfig()->getTotalMemReqs() << std::endl;
        std::cout << "VAL GEN: " << inst_ptr->getVectorMemConfig()->getNumMemReqsGenerated() << std::endl;
        std::cout << "VAL COMPLETED: " << inst_ptr->getVectorMemConfig()->getNumMemReqsCompleted() << std::endl;

        // THIS ASSERT FAILS
        EXPECT_TRUE(inst_ptr->getVectorMemConfig()->getTotalMemReqs() == expected_val);
    }

  private:
    olympia::VLSU* vlsu_;
};

void runTests(int argc, char** argv)
{
    DEFAULTS.auto_summary_default = "off";
    std::string input_file;

    sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
    auto & app_opts = cls.getApplicationOptions();
    app_opts.add_options()(
        "input-file",
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
    OlympiaSim sim("simple", scheduler, num_cores, input_file, ilimit, show_factories);
    sparta::RootTreeNode* root_node = sim.getRoot();
    cls.populateSimulation(&sim);

    olympia::VLSU* my_vlsu = root_node->getChild("cpu.core0.vlsu")->getResourceAs<olympia::VLSU*>();
    olympia::VLSUTester vlsu_tester{my_vlsu};

    if (input_file.find("vlsu_load.json") != std::string::npos)
    {
        // Test VLSU
        cls.runSimulator(&sim, 68);

        vlsu_tester.test_mem_request_count(12);
    }
    else if (input_file.find("vlsu_store.json") != std::string::npos)
    {
        // Test VLSU
        cls.runSimulator(&sim, 41);
        vlsu_tester.test_mem_request_count(16);
    }
    else
    {
        sparta_assert(false, "Invalid input file: " << input_file);
    }
}

int main(int argc, char** argv)
{
    runTests(argc, argv);

    REPORT_ERROR;
    return (int)ERROR_CODE;
}
