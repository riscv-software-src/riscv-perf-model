
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

class olympia::DecodeTester
{
public:
    DecodeTester(olympia::Decode * decode) :
        decode_(decode)
    {}

    void test_waiting_on_vset()
    {
        EXPECT_TRUE(decode_->waiting_on_vset_ == true);
    }

    void test_waiting_on_vset(const bool expected_val)
    {
        EXPECT_TRUE(decode_->waiting_on_vset_ == expected_val);
    }

    void test_vl(const uint32_t expected_vl)
    {
        EXPECT_TRUE(decode_->VectorConfig_.vl == expected_vl);
    }

    void test_sew(const uint32_t expected_sew)
    {
        EXPECT_TRUE(decode_->VectorConfig_.sew == expected_sew);
    }

    void test_lmul(const uint32_t expected_lmul)
    {
        EXPECT_TRUE(decode_->VectorConfig_.lmul == expected_lmul);
    }

    void test_vlmax(const uint32_t expected_vlmax)
    {
        EXPECT_TRUE(decode_->VectorConfig_.vlmax == expected_vlmax);
    }

    void test_vta(const bool expected_vta)
    {
        EXPECT_TRUE(decode_->VectorConfig_.vta == expected_vta);
    }

private:
    olympia::Decode * decode_;
};

class olympia::ROBTester
{
public:
    ROBTester(olympia::ROB * rob) :
        rob_(rob)
    {}

    void test_num_insts_retired(const uint64_t expected_num_insts_retired)
    {
        EXPECT_TRUE(rob_->num_retired_ == expected_num_insts_retired);
    }

    void test_num_uops_retired(const uint64_t expected_num_uops_retired)
    {
        EXPECT_TRUE(rob_->num_uops_retired_ == expected_num_uops_retired);
    }

    void test_last_inst_has_tail(const bool expected_tail)
    {
        EXPECT_TRUE(rob_->last_inst_retired_ != nullptr);
        EXPECT_TRUE(rob_->last_inst_retired_->hasTail() == expected_tail);
    }

private:
    olympia::ROB * rob_;
};

void runTests(int argc, char **argv)
    {
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
    if (!cls.parse(argc, argv, err_code))
    {
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

    olympia::Decode *my_decode = \
        root_node->getChild("cpu.core0.decode")->getResourceAs<olympia::Decode*>();
    olympia::DecodeTester decode_tester {my_decode};

    olympia::ROB *my_rob = \
        root_node->getChild("cpu.core0.rob")->getResourceAs<olympia::ROB *>();
    olympia::ROBTester rob_tester {my_rob};

    if (input_file.find("vsetivli_vaddvv.json") != std::string::npos)
    {
        // Test Decode (defaults)
        decode_tester.test_lmul(1);
        decode_tester.test_vl(128);
        decode_tester.test_vta(false);
        decode_tester.test_sew(8);
        decode_tester.test_vlmax(128);

        cls.runSimulator(&sim);

        // Test after vsetivli
        decode_tester.test_waiting_on_vset(false);
        decode_tester.test_lmul(4);
        decode_tester.test_vl(512);
        decode_tester.test_vta(false);
        decode_tester.test_sew(8);
        decode_tester.test_vlmax(512);

        // Test Retire
        rob_tester.test_num_insts_retired(2);
        // vset + 4 vadd.vv uops
        rob_tester.test_num_uops_retired(5);
        rob_tester.test_last_inst_has_tail(false);
    }
    else if(input_file.find("vsetvli_vaddvv.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        // Test Decode
        decode_tester.test_waiting_on_vset(false);
        decode_tester.test_lmul(1);
        decode_tester.test_vl(32);
        decode_tester.test_vta(true);
        decode_tester.test_sew(32);
        decode_tester.test_vlmax(32);

        // Test Retire
        rob_tester.test_num_insts_retired(2);
        // vset + 1 vadd.vv uop
        rob_tester.test_num_uops_retired(2);
        rob_tester.test_last_inst_has_tail(false);
    }
    else if(input_file.find("vsetvl_vaddvv.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        // Test Decode
        decode_tester.test_waiting_on_vset(false);
        decode_tester.test_lmul(1);
        decode_tester.test_vl(16);
        decode_tester.test_vta(true);
        decode_tester.test_sew(64);
        decode_tester.test_vlmax(16);

        // Test Retire
        rob_tester.test_num_insts_retired(2);
        // vset + 1 vadd.vv uop
        rob_tester.test_num_uops_retired(2);
        rob_tester.test_last_inst_has_tail(false);
    }
    else if(input_file.find("vsetivli_vaddvv_tail.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        // Test Decode
        decode_tester.test_lmul(8);
        decode_tester.test_vl(900);
        decode_tester.test_vta(false);
        decode_tester.test_sew(8);
        decode_tester.test_vlmax(1024);

        // Test Retire
        rob_tester.test_num_insts_retired(2);
        // vset + 8 vadd.vv uop
        rob_tester.test_num_uops_retired(9);
        rob_tester.test_last_inst_has_tail(true);
    }
    else if(input_file.find("multiple_vset.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        // Test Decode (last vset)
        decode_tester.test_waiting_on_vset(false);
        decode_tester.test_lmul(8);
        decode_tester.test_vl(1024);
        decode_tester.test_vta(false);
        decode_tester.test_sew(8);
        decode_tester.test_vlmax(1024);

        // Test Retire
        rob_tester.test_num_insts_retired(8);
        // vset + 1 vadd.vv + vset + 2 vadd.vv + vset + 4 vadd.vv uop + vset + 8 vadd.vv
        rob_tester.test_num_uops_retired(19);
    }
    else if(input_file.find("vmulvx.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        // Test Retire
        rob_tester.test_num_insts_retired(3);
        // vadd + 4 vmul.vx uop
        rob_tester.test_num_uops_retired(6);
        rob_tester.test_last_inst_has_tail(false);

        // TODO: Test source values for all uops
    }
    else if(input_file.find("vwmulvv.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        // Test Retire
        rob_tester.test_num_insts_retired(2);
        // vadd + 8 vwmul.vv uop
        rob_tester.test_num_uops_retired(9);
        rob_tester.test_last_inst_has_tail(false);

        // TODO: Test destination values for all uops
    }
    else if(input_file.find("vmseqvv.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        // Test Retire
        rob_tester.test_num_insts_retired(2);
        // vadd + 4 vmseq.vv uops
        rob_tester.test_num_uops_retired(5);
        rob_tester.test_last_inst_has_tail(false);

        // TODO: Test destination values for all uops
    }
    else if(input_file.find("vrgather.json") != std::string::npos)
    {
        // Unsupported vector instructions are expected to make the simulator to throw
        bool sparta_exception_fired = false;
        try {
            cls.runSimulator(&sim);
        } catch (const sparta::SpartaException& ex) {
            sparta_exception_fired = true;
        }
        EXPECT_TRUE(sparta_exception_fired);
    }
}

int main(int argc, char **argv)
    {
    runTests(argc, argv);

    REPORT_ERROR;
    return (int)ERROR_CODE;
}
