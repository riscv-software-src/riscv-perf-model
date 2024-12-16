#include "OlympiaSim.hpp"
#include "Decode.hpp"
#include "ROB.hpp"
#include "VectorUopGenerator.hpp"

#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT

const char USAGE[] = "Usage:\n"
                     "\n"
                     "\n";

sparta::app::DefaultValues DEFAULTS;

class olympia::DecodeTester
{
  public:
    DecodeTester(olympia::Decode* decode) : decode_(decode) {}

    void test_waiting_on_vset() { EXPECT_TRUE(decode_->waiting_on_vset_ == true); }

    void test_waiting_on_vset(const bool expected_val)
    {
        EXPECT_TRUE(decode_->waiting_on_vset_ == expected_val);
    }

    void test_vl(const uint32_t expected_vl)
    {
        EXPECT_TRUE(decode_->vector_config_->getVL() == expected_vl);
    }

    void test_sew(const uint32_t expected_sew)
    {
        EXPECT_TRUE(decode_->vector_config_->getSEW() == expected_sew);
    }

    void test_lmul(const uint32_t expected_lmul)
    {
        EXPECT_TRUE(decode_->vector_config_->getLMUL() == expected_lmul);
    }

    void test_vlmax(const uint32_t expected_vlmax)
    {
        EXPECT_TRUE(decode_->vector_config_->getVLMAX() == expected_vlmax);
    }

    void test_vta(const bool expected_vta)
    {
        EXPECT_TRUE(decode_->vector_config_->getVTA() == expected_vta);
    }

  private:
    olympia::Decode* decode_;
};

class olympia::ROBTester
{
  public:
    ROBTester(olympia::ROB* rob) : rob_(rob) {}

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
    olympia::ROB* rob_;
};

class olympia::VectorUopGeneratorTester
{
  public:
    VectorUopGeneratorTester(olympia::VectorUopGenerator* vuop) : vuop_{vuop} {}

    void test_num_vuops_generated(const uint64_t expected_num_vuops_generated)
    {
        EXPECT_TRUE(vuop_->vuops_generated_ == expected_num_vuops_generated);
    }

  private:
    VectorUopGenerator* vuop_;
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
        sparta_assert(false, "Command line parsing failed");
    }

    sparta::Scheduler scheduler;
    uint32_t num_cores = 1;
    uint64_t ilimit = 0;
    bool show_factories = false;
    OlympiaSim sim("simple", scheduler, num_cores, input_file, ilimit, show_factories);
    sparta::RootTreeNode* root_node = sim.getRoot();
    cls.populateSimulation(&sim);

    auto* my_decode = root_node->getChild("cpu.core0.decode")->getResourceAs<olympia::Decode*>();
    olympia::DecodeTester decode_tester{my_decode};

    auto* my_rob = root_node->getChild("cpu.core0.rob")->getResourceAs<olympia::ROB*>();
    olympia::ROBTester rob_tester{my_rob};

    auto* my_vuop_generator = root_node->getChild("cpu.core0.decode.vec_uop_gen")
                                  ->getResourceAs<olympia::VectorUopGenerator*>();
    olympia::VectorUopGeneratorTester vuop_tester{my_vuop_generator};

    if (input_file.find("vsetivli_vaddvv_e8m4.json") != std::string::npos)
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
    else if (input_file.find("vsetvli_vaddvv_e32m1ta.json") != std::string::npos)
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
    else if (input_file.find("vsetvl_vaddvv_e64m1ta.json") != std::string::npos)
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
    else if (input_file.find("vsetivli_vaddvv_tail_e8m8ta.json") != std::string::npos)
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
    else if (input_file.find("multiple_vset.json") != std::string::npos)
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
    else if (input_file.find("vmulvx_e8m4.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        // Test Retire
        rob_tester.test_num_insts_retired(3);
        // vadd + 4 vmul.vx uop
        rob_tester.test_num_uops_retired(6);
        rob_tester.test_last_inst_has_tail(false);

        // TODO: Test source values for all uops
    }
    else if (input_file.find("vwmulvv_e8m4.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        // Test Retire
        rob_tester.test_num_insts_retired(2);
        // vadd + 8 vwmul.vv uop
        rob_tester.test_num_uops_retired(9);
        rob_tester.test_last_inst_has_tail(false);

        // TODO: Test destination values for all uops
    }
    else if (input_file.find("vmseqvv_e8m4.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        // Test Retire
        rob_tester.test_num_insts_retired(2);
        // vadd + 4 vmseq.vv uops
        rob_tester.test_num_uops_retired(5);
        rob_tester.test_last_inst_has_tail(false);

        // TODO: Test destination values for all uops
    }
    else if (input_file.find("vrgather.json") != std::string::npos)
    {
        // Unsupported vector instructions are expected to make the simulator to throw
        bool sparta_exception_fired = false;
        try
        {
            cls.runSimulator(&sim);
        }
        catch (const sparta::SpartaException & ex)
        {
            sparta_exception_fired = true;
        }
        EXPECT_TRUE(sparta_exception_fired);
    }
    else if (input_file.find("vsadd.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        rob_tester.test_num_insts_retired(2);
        rob_tester.test_num_uops_retired(5);
        rob_tester.test_last_inst_has_tail(false);

        vuop_tester.test_num_vuops_generated(4);
    }
    else if (input_file.find("elementwise.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        decode_tester.test_lmul(4);
        decode_tester.test_vl(256);
        decode_tester.test_vta(false);
        decode_tester.test_sew(32);
        decode_tester.test_vlmax(128);

        vuop_tester.test_num_vuops_generated(4);
    }
    else if (input_file.find("widening.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        decode_tester.test_lmul(4);
        decode_tester.test_vl(256);
        decode_tester.test_vta(false);
        decode_tester.test_sew(32);
        decode_tester.test_vlmax(128);

        vuop_tester.test_num_vuops_generated(8);
    }
    else if (input_file.find("widening_mixed.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        decode_tester.test_lmul(4);
        decode_tester.test_vl(256);
        decode_tester.test_vta(false);
        decode_tester.test_sew(32);
        decode_tester.test_vlmax(128);

        vuop_tester.test_num_vuops_generated(8);
    }
    else if (input_file.find("mac.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        decode_tester.test_lmul(4);
        decode_tester.test_vl(256);
        decode_tester.test_vta(false);
        decode_tester.test_sew(32);
        decode_tester.test_vlmax(128);

        vuop_tester.test_num_vuops_generated(4);
    }
    else if (input_file.find("mac_widening.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        decode_tester.test_lmul(4);
        decode_tester.test_vl(256);
        decode_tester.test_vta(false);
        decode_tester.test_sew(32);
        decode_tester.test_vlmax(128);

        vuop_tester.test_num_vuops_generated(8);
    }
    else if (input_file.find("single_dest.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        decode_tester.test_lmul(4);
        decode_tester.test_vl(256);
        decode_tester.test_vta(false);
        decode_tester.test_sew(32);
        decode_tester.test_vlmax(128);

        vuop_tester.test_num_vuops_generated(4);
    }
    else if (input_file.find("narrowing.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        decode_tester.test_lmul(4);
        decode_tester.test_vl(256);
        decode_tester.test_vta(false);
        decode_tester.test_sew(32);
        decode_tester.test_vlmax(128);

        vuop_tester.test_num_vuops_generated(8);
    }
    else if (input_file.find("int_ext.json") != std::string::npos)
    {
        cls.runSimulator(&sim);

        decode_tester.test_lmul(4);
        decode_tester.test_vl(256);
        decode_tester.test_vta(false);
        decode_tester.test_sew(64);
        decode_tester.test_vlmax(64);

        vuop_tester.test_num_vuops_generated(12);
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
