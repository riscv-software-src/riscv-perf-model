
#include "Dispatch_test.hpp"

TEST_INIT

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
