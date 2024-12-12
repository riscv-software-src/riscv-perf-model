
#include "CPUFactory.hpp"
#include "CoreUtils.hpp"
#include "dispatch/Dispatch.hpp"
#include "decode/MavisUnit.hpp"
#include "OlympiaAllocators.hpp"
#include "OlympiaSim.hpp"
#include "execute/IssueQueue.hpp"
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

class olympia::IssueQueueTester {
public:
  void test_occupied(olympia::IssueQueue &issuequeue) {
    // testing RAW dependency for ExecutePipe
    // only alu0 should have an issued instruction
    // so alu0's total_insts_issued should be 1
    EXPECT_TRUE(issuequeue.total_insts_issued_ == 1);
  }
  void test_empty(olympia::IssueQueue &issuequeue) {
    // testing RAW dependency for ExecutePipe
    // only alu0 should have an issued instruction
    // so alu0's total_insts_issued should be 1
    EXPECT_TRUE(issuequeue.total_insts_issued_ == 0);
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
  sparta::Scheduler sched;

  if (input_file.find("test_int_pipe.json") != std::string::npos) {
    DispatchSim sim(&sched, "mavis_isa_files", "arch/isa_json", datafiles[0],
                    input_file, enable_vector);
    sparta::RootTreeNode *root_node = sim.getRoot();
    cls.populateSimulation(&sim);
    cls.runSimulator(&sim, 4);
    olympia::IssueQueue *my_issuequeue =
        root_node->getChild("execute.iq0")
            ->getResourceAs<olympia::IssueQueue *>();
    olympia::IssueQueue *my_issuequeue1 =
        root_node->getChild("execute.iq1")
            ->getResourceAs<olympia::IssueQueue *>();
    olympia::IssueQueue *my_issuequeue2 =
        root_node->getChild("execute.iq2")
            ->getResourceAs<olympia::IssueQueue *>();
    olympia::IssueQueueTester issuequeue_tester;

    issuequeue_tester.test_occupied(*my_issuequeue);
    issuequeue_tester.test_empty(*my_issuequeue1);
    issuequeue_tester.test_empty(*my_issuequeue2);
  } else if (input_file.find("test_mul_pipe.json") != std::string::npos) {
    DispatchSim sim(&sched, "mavis_isa_files", "arch/isa_json", datafiles[0],
                    input_file, enable_vector);
    sparta::RootTreeNode *root_node = sim.getRoot();
    cls.populateSimulation(&sim);
    cls.runSimulator(&sim, 6);
    olympia::IssueQueue *my_issuequeue =
        root_node->getChild("execute.iq0")
            ->getResourceAs<olympia::IssueQueue *>();
    olympia::IssueQueue *my_issuequeue1 =
        root_node->getChild("execute.iq1")
            ->getResourceAs<olympia::IssueQueue *>();
    olympia::IssueQueue *my_issuequeue2 =
        root_node->getChild("execute.iq2")
            ->getResourceAs<olympia::IssueQueue *>();
    olympia::IssueQueueTester issuequeue_tester;
    issuequeue_tester.test_occupied(*my_issuequeue1);
    issuequeue_tester.test_empty(*my_issuequeue);
    issuequeue_tester.test_empty(*my_issuequeue2);
  } else if (input_file.find("test_two_int_pipe.json") != std::string::npos) {
    uint64_t ilimit = 0;
    uint32_t num_cores = 1;
    bool show_factories = false;
    OlympiaSim full_sim("simple", sched,
                        num_cores, // cores
                        input_file, ilimit, show_factories);
    cls.populateSimulation(&full_sim);
    sparta::RootTreeNode *root_node = full_sim.getRoot();
    cls.runSimulator(&full_sim, 10);
    olympia::IssueQueue *my_issuequeue =
        root_node->getChild("cpu.core0.execute.iq0")
            ->getResourceAs<olympia::IssueQueue *>();
    olympia::IssueQueue *my_issuequeue1 =
        root_node->getChild("cpu.core0.execute.iq1")
            ->getResourceAs<olympia::IssueQueue *>();
    olympia::IssueQueue *my_issuequeue2 =
        root_node->getChild("cpu.core0.execute.iq2")
            ->getResourceAs<olympia::IssueQueue *>();
    olympia::IssueQueueTester issuequeue_tester;
    issuequeue_tester.test_occupied(*my_issuequeue1);
    issuequeue_tester.test_occupied(*my_issuequeue);
    issuequeue_tester.test_empty(*my_issuequeue2);
  }
}

int main(int argc, char **argv) {
  runIQTest(argc, argv);

  REPORT_ERROR;
  return (int)ERROR_CODE;
}
