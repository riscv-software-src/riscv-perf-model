
#include "CPUFactory.hpp"
#include "CoreUtils.hpp"
#include "Dispatch.hpp"
#include "MavisUnit.hpp"
#include "OlympiaAllocators.hpp"
#include "OlympiaSim.hpp"
#include "Rename.hpp"

#include "test/core/common/SinkUnit.hpp"
#include "test/core/common/SourceUnit.hpp"
#include "test/core/dispatch/Dispatch_test.hpp"
#include "test/core/dispatch/ExecutePipeSinkUnit.hpp"
#include "test/core/rename/ROBSinkUnit.hpp"

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

// The main tester of Dispatch.  The test is encapsulated in the
// parameter test_type of the Source unit.
void runTest(int argc, char **argv) {
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
  DispatchSim sim(&sched, "mavis_isa_files", "arch/isa_json", datafiles[0],
                  input_file, enable_vector);

  cls.populateSimulation(&sim);
  cls.runSimulator(&sim);

  EXPECT_FILES_EQUAL(datafiles[0],
                     "expected_output/" + datafiles[0] + ".EXPECTED");
}

int main(int argc, char **argv) {
  runTest(argc, argv);

  REPORT_ERROR;
  return (int)ERROR_CODE;
}
