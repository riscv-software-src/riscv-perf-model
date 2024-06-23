
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

class olympia::DecodeTester {
public:
  void test_waiting_on_vset(olympia::Decode &decode) {
    EXPECT_TRUE(decode.waiting_on_vset_ == true);
  }
  void test_no_waiting_on_vset(olympia::Decode &decode) {
    // test waiting on vset is false
    EXPECT_TRUE(decode.waiting_on_vset_ == false);
  }
  void test_VCSRs(olympia::Decode &decode) {
    // test VCSRs
    EXPECT_TRUE(decode.VCSRs_.lmul == 1);
    EXPECT_TRUE(decode.VCSRs_.vl == 128);
    EXPECT_TRUE(decode.VCSRs_.vta == 0);
    EXPECT_TRUE(decode.VCSRs_.sew == 8);
    EXPECT_TRUE(decode.VCSRs_.vlmax == 128);
  }

  void test_VCSRs_after(olympia::Decode &decode) {
    // test VCSRs
    EXPECT_TRUE(decode.VCSRs_.lmul == 4);
    EXPECT_TRUE(decode.VCSRs_.vl == 1024);
    EXPECT_TRUE(decode.VCSRs_.vta == 0);
    EXPECT_TRUE(decode.VCSRs_.sew == 8);
  }

  void test_VCSRs_sew_32(olympia::Decode &decode) {
    // test VCSRs
    EXPECT_TRUE(decode.VCSRs_.lmul == 1);
    EXPECT_TRUE(decode.VCSRs_.vl == 512);
    EXPECT_TRUE(decode.VCSRs_.vta == 1);
    EXPECT_TRUE(decode.VCSRs_.sew == 32);
  }

  void test_vl_max(olympia::Decode &decode){
    EXPECT_TRUE(decode.VCSRs_.vl == 512);
    EXPECT_TRUE(decode.VCSRs_.lmul == 8);
    EXPECT_TRUE(decode.VCSRs_.vta == 1);
    EXPECT_TRUE(decode.VCSRs_.sew == 16);
  }

  void test_VCSRs_mul_vset(olympia::Decode &decode){
    EXPECT_TRUE(decode.VCSRs_.vl == 128);
    EXPECT_TRUE(decode.VCSRs_.lmul == 1);
    EXPECT_TRUE(decode.VCSRs_.vta == 0);
    EXPECT_TRUE(decode.VCSRs_.sew == 32);
  }
};

class olympia::IssueQueueTester {
public:
  void test_uop_count(olympia::IssueQueue &issuequeue) {
    // 4 UOps + 1 vset instruction, so 5 total
    EXPECT_TRUE(issuequeue.total_insts_issued_ == 5);
  }

  void test_no_uop_count(olympia::IssueQueue &issuequeue) {
    // 2 instructions, no uops
    EXPECT_TRUE(issuequeue.total_insts_issued_ == 2);
  }

  void no_inst_issued(olympia::IssueQueue &issuequeue) {
    EXPECT_TRUE(issuequeue.total_insts_issued_ == 0);
  }
};

class olympia::RenameTester{
  public:
    void test_hastail(olympia::Rename &rename){
      const auto & renaming_inst = rename.uop_queue_.read(0);
      EXPECT_TRUE(renaming_inst->hasTail() == true);
    }
};
void runIQTest(int argc, char **argv) {
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
  pos_opts.add("output_file",
               -1); // example, look for the <data file> at the end

  int err_code = 0;
  if (!cls.parse(argc, argv, err_code)) {
    sparta_assert(
        false,
        "Command line parsing failed"); // Any errors already printed to cerr
  }

  sparta_assert(false == datafiles.empty(),
                "Need an output file as the last argument of the test");
  sparta::Scheduler scheduler;

  uint64_t ilimit = 0;
  uint32_t num_cores = 1;
  bool show_factories = false;
  OlympiaSim sim("simple", scheduler,
                 num_cores, // cores
                 input_file, ilimit, show_factories);

  if (input_file.find("vsetivli_vadd_lmul_4.json") != std::string::npos) {
    sparta::RootTreeNode *root_node = sim.getRoot();
    cls.populateSimulation(&sim);
    olympia::Decode *my_decode =
        root_node->getChild("cpu.core0.decode")
            ->getResourceAs<olympia::Decode*>();
    olympia::DecodeTester decode_tester;

    decode_tester.test_VCSRs(*my_decode);
    cls.runSimulator(&sim, 3);
    // decode_tester.test_no_waiting_on_vset(*my_decode);
    // decode_tester.test_VCSRs_after(*my_decode);
    // cls.runSimulator(&sim, 8);
    // decode_tester.test_VCSRs_after(*my_decode);
    // cls.runSimulator(&sim);
    // olympia::IssueQueue *my_issuequeue =
    //     root_node->getChild("cpu.core0.execute.iq5")
    //         ->getResourceAs<olympia::IssueQueue *>();

    // olympia::IssueQueueTester issue_queue_tester;
    // issue_queue_tester.test_uop_count(*my_issuequeue);
  }
  else if(input_file.find("vsetvli_vadd_sew_32.json") != std::string::npos){
    sparta::RootTreeNode *root_node = sim.getRoot();
    cls.populateSimulation(&sim);
    cls.runSimulator(&sim, 8);
    olympia::Decode *my_decode =
        root_node->getChild("cpu.core0.decode")
            ->getResourceAs<olympia::Decode*>();
    olympia::DecodeTester decode_tester;
    decode_tester.test_VCSRs_sew_32(*my_decode);
    cls.runSimulator(&sim);
    olympia::IssueQueue *my_issuequeue =
        root_node->getChild("cpu.core0.execute.iq5")
            ->getResourceAs<olympia::IssueQueue *>();

    olympia::IssueQueueTester issue_queue_tester;
    issue_queue_tester.test_no_uop_count(*my_issuequeue);
  }
  else if(input_file.find("vsetvl_vadd.json") != std::string::npos)
  {
    sparta::RootTreeNode *root_node = sim.getRoot();
    cls.populateSimulation(&sim);
    cls.runSimulator(&sim, 4);
    olympia::Decode *my_decode =
        root_node->getChild("cpu.core0.decode")
            ->getResourceAs<olympia::Decode*>();
    olympia::DecodeTester decode_tester;
    decode_tester.test_waiting_on_vset(*my_decode);
  }
  else if(input_file.find("vsetvli_vl_max_setting.json") != std::string::npos){
    // lmul = 8
    // SEW = 16

    sparta::RootTreeNode *root_node = sim.getRoot();
    cls.populateSimulation(&sim);
    cls.runSimulator(&sim, 8);
    olympia::Decode *my_decode =
        root_node->getChild("cpu.core0.decode")
            ->getResourceAs<olympia::Decode*>();
    olympia::DecodeTester decode_tester;
    decode_tester.test_vl_max(*my_decode);
  }
  else if(input_file.find("multiple_vset.json") != std::string::npos){
    sparta::RootTreeNode *root_node = sim.getRoot();
    cls.populateSimulation(&sim);
    olympia::Decode *my_decode =
        root_node->getChild("cpu.core0.decode")
            ->getResourceAs<olympia::Decode*>();
    olympia::DecodeTester decode_tester;
    cls.runSimulator(&sim, 21);
    decode_tester.test_VCSRs_mul_vset(*my_decode);
  }
  else if(input_file.find("vmul_transfer.json") != std::string::npos){
    sparta::RootTreeNode *root_node = sim.getRoot();
    cls.populateSimulation(&sim);
    cls.runSimulator(&sim, 7);
    olympia::IssueQueue *my_issuequeue =
        root_node->getChild("cpu.core0.execute.iq5")
            ->getResourceAs<olympia::IssueQueue *>();

    olympia::IssueQueueTester issue_queue_tester;
    // vmul.vx relies on scalar add, should not process until it's done RAW hazard
    issue_queue_tester.no_inst_issued(*my_issuequeue);
  }
  else if(input_file.find("undisturbed_checking.json") != std::string::npos){
    sparta::RootTreeNode *root_node = sim.getRoot();
    cls.populateSimulation(&sim);
    cls.runSimulator(&sim, 9);
    olympia::Rename *my_rename = root_node->getChild("cpu.core0.rename")
                                     ->getResourceAs<olympia::Rename *>();
    olympia::RenameTester rename_tester;
    rename_tester.test_hastail(*my_rename);
    
  }
}

int main(int argc, char **argv) {
  runIQTest(argc, argv);

  REPORT_ERROR;
  return (int)ERROR_CODE;
}