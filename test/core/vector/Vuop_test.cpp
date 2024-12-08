#include "OlympiaSim.hpp"
#include "Decode.hpp"
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

    auto* my_vuop_generator = root_node->getChild("cpu.core0.decode.vec_uop_gen")
                                  ->getResourceAs<olympia::VectorUopGenerator*>();
    olympia::VectorUopGeneratorTester vuop_tester{my_vuop_generator};

    if (input_file.find("elementwise.json") != std::string::npos)
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
