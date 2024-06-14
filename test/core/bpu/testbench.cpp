// -----------------------------------------------------------------------
//
//                i_dut_flush ---
//                              |
//    |--------|             |--------|               |--------|
//    |        |---------A->>|        |-----------C->>|        |
//    | src    |             |  dut   |               |  sink  |
//    |        |<<-B---------|        |<<-D-----------|        |
//    |--------|             |--------|               |--------|
//
//
// src.ports.o_instgrp_write  ----A->> dut.ports.i_instgrp_write
// src.ports.i_credits        <<--B--- dut.ports.o_restore_credits
// 
// dut.ports.o_instgrp_write  ----C->> sink.ports.i_instgrp_write
// dut.ports.i_credits        <<--D--- sink.ports.o_restore_credits
//
// -----------------------------------------------------------------------
#include "dut.h"
#include "sink.h"
#include "src.h"
#include "commonTypes.h"
#include "uarch/SimpleBTB.hpp"
#include "uarch/Gem5Tage.hpp"

#include "MavisUnit.hpp"
#include "OlympiaAllocators.hpp"
#include "OlympiaSim.hpp"

#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <iostream>
#include <vector>

using namespace std;

TEST_INIT

olympia::InstAllocator inst_allocator(2000, 1000);

// ----------------------------------------------------------------------
// Testbench reference - starting point for CAM unit benches
//
// ----------------------------------------------------------------------
class Simulator : public sparta::app::Simulation
{
public:
  Simulator(sparta::Scheduler* sched,
            const string & mavis_isa_files,
            const string & mavis_uarch_files,
            const string & output_file,
            const string & input_file)
    : sparta::app::Simulation("Simulator", sched),
      input_file_(input_file),
      test_tap_(getRoot(), "info", output_file)
  {
  }

  ~Simulator() { getRoot()->enterTeardown(); }

  void runRaw(uint64_t run_time) override final
  {
    (void)run_time; // ignored

    sparta::app::Simulation::runRaw(run_time);
  }


private:
  void buildTree_() override
  {

    auto rtn = getRoot();
    allocators_tn_.reset(new olympia::OlympiaAllocators(rtn));

    // Create a Mavis Unit
    tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(rtn,
                                    olympia::MavisUnit::name,
                                    sparta::TreeNode::GROUP_NAME_NONE,
                                    sparta::TreeNode::GROUP_IDX_NONE,
                                    "Mavis Unit",
                                    &mavis_fact));

    // Create a Source Unit -- this would represent Rename
    sparta::ResourceTreeNode* src = new sparta::ResourceTreeNode(rtn,
                                    "src",
                                    sparta::TreeNode::GROUP_NAME_NONE,
                                    sparta::TreeNode::GROUP_IDX_NONE,
                                    "Source Unit",
                                    &source_fact);

    tns_to_delete_.emplace_back(src);

    auto* src_params = src->getParameterSet();
    src_params->getParameter("input_file")->setValueFromString(input_file_);

    // Create DUT
    sparta::ResourceTreeNode* dut = new sparta::ResourceTreeNode(rtn,
                                    "dut",
                                    sparta::TreeNode::GROUP_NAME_NONE,
                                    sparta::TreeNode::GROUP_IDX_NONE,
                                    "DUT",
                                    &dut_fact);

    tns_to_delete_.emplace_back(dut);

    // Create BTB BPU
    sparta::ResourceTreeNode* simplebtb = new sparta::ResourceTreeNode(rtn,
                                    "simplebtb",
                                    sparta::TreeNode::GROUP_NAME_NONE,
                                    sparta::TreeNode::GROUP_IDX_NONE,
                                    "SIMPLEBTB",
                                    &simplebtb_fact);

    tns_to_delete_.emplace_back(simplebtb);

    // Create Gem5Tage BPU
    sparta::ResourceTreeNode* gem5tage = new sparta::ResourceTreeNode(rtn,
                                    "gem5tage",
                                    sparta::TreeNode::GROUP_NAME_NONE,
                                    sparta::TreeNode::GROUP_IDX_NONE,
                                    "GEM5TAGE",
                                    &tagebase_fact);

    tns_to_delete_.emplace_back(gem5tage);

    // Create SinkUnit 
    sparta::ResourceTreeNode* sink = new sparta::ResourceTreeNode(rtn,
                                    "sink",
                                    sparta::TreeNode::GROUP_NAME_NONE,
                                    sparta::TreeNode::GROUP_IDX_NONE,
                                    "Sink Unit",
                                    &sink_fact);

    tns_to_delete_.emplace_back(sink);

    auto* sink_params = sink->getParameterSet();
    sink_params->getParameter("purpose")->setValueFromString("grp");

  }

  void configureTree_() override {}

  void bindTree_() override
  {
    auto* root_node = getRoot();

    // A - src sends instgrp to dut
    sparta::bind(root_node->getChildAs<sparta::Port>(
                 "dut.ports.i_instgrp_write"),
                 root_node->getChildAs<sparta::Port>(
                 "src.ports.o_instgrp_write"));

    // B - dut gives credits back to source
    sparta::bind(root_node->getChildAs<sparta::Port>(
                 "dut.ports.o_restore_credits"),
                 root_node->getChildAs<sparta::Port>(
                 "src.ports.i_credits"));

    // Cx - dut sends instgrp to sink
    sparta::bind(root_node->getChildAs<sparta::Port>(
                 "dut.ports.o_instgrp_write"),
                 root_node->getChildAs<sparta::Port>(
                 "sink.ports.i_instgrp_write"));

    // Dx - sink gives credits back to dut
    sparta::bind(root_node->getChildAs<sparta::Port>(
                 "dut.ports.i_credits"), 
                 root_node->getChildAs<sparta::Port>(
                 "sink.ports.o_restore_credits"));     

    // -----------------------------------------------
    // Only invalidate is sent to simpleBTB
    // if this model is interesting it would mimic
    // what is done for (L)TAGE
    // X - dut sends invalidate to simplebtb
    // -----------------------------------------------
    sparta::bind(root_node->getChildAs<sparta::Port>(
                 "simplebtb.ports.i_bpu_invalidate"),
                 root_node->getChildAs<sparta::Port>(
                 "dut.ports.o_bpu_invalidate"));

    // -----------------------------------------------
    // Request to the BPU for prediction/etc
    // The interface uses a command struct
    // -----------------------------------------------
    // X - dut sends request to gem5tage
    sparta::bind(root_node->getChildAs<sparta::Port>(
                 "gem5tage.ports.i_bpu_request"),
                 root_node->getChildAs<sparta::Port>(
                 "dut.ports.o_bpu_request"));

    // Dx - bpu send response back to dut
    sparta::bind(root_node->getChildAs<sparta::Port>(
                 "dut.ports.i_bpu_response"), 
                 root_node->getChildAs<sparta::Port>(
                 "gem5tage.ports.o_bpu_response"));     
  }

  // Allocators
  std::unique_ptr<olympia::OlympiaAllocators> allocators_tn_;

  olympia::MavisFactory mavis_fact;

  using DutFactory = sparta::ResourceFactory<olympia::Dut,
                             olympia::Dut::DutParameterSet>;

  DutFactory dut_fact;

  using SimpleBTBFactory = sparta::ResourceFactory<olympia::SimpleBTB,
                           olympia::SimpleBTB::SimpleBTBParameterSet>;

  SimpleBTBFactory simplebtb_fact;

  using Gem5TageFactory = sparta::ResourceFactory<olympia::Gem5Tage,
                           olympia::Gem5Tage::Gem5TageParameterSet>;

  Gem5TageFactory tagebase_fact;

  core_test::SrcFactory  source_fact;
  core_test::SinkFactory sink_fact;

  vector<unique_ptr<sparta::TreeNode>> tns_to_delete_;
  vector<sparta::ResourceTreeNode*>    exe_units_;

  const string input_file_;
  sparta::log::Tap  test_tap_;
};

const char USAGE[] =
    "Usage:\n\n"
    "Testbench options \n"
    "    [ --input_file ]   : json or stf input file\n"
    "    [ --output_file ]  : output file for results checking\n"
    " \n"
    "Commonly used options \n"
    "    [-i insts] [-r RUNTIME] [--show-tree] [--show-dag]\n"
    "    [-p PATTERN VAL] [-c FILENAME]\n"
    "    [-l PATTERN CATEGORY DEST]\n"
    "    [-h,--help] <workload [stf trace or JSON]>\n"
    "\n";

sparta::app::DefaultValues DEFAULTS;
// ----------------------------------------------------------------
// ----------------------------------------------------------------
bool runTest(int argc, char** argv)
{
  DEFAULTS.auto_summary_default = "off";
  vector<string> datafiles;
  string input_file;

  sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
  auto & app_opts = cls.getApplicationOptions();

  app_opts.add_options()
    ("output_file",
     sparta::app::named_value<vector<string>>("output_file", &datafiles),
      "Specifies the output file")

    ("input_file",
      sparta::app::named_value<string>("INPUT_FILE", &input_file)
          ->default_value(""),
      "Provide a JSON or STF instruction stream")
  ;

  po::positional_options_description & pos_opts = cls.getPositionalOptions();
  // example, look for the <data file> at the end
  pos_opts.add("output_file", -1);

  int err_code = 0;

  if (!cls.parse(argc, argv, err_code))
  {
    sparta_assert(false,"Command line parsing failed");
  }

  auto& vm = cls.getVariablesMap();
  if(vm.count("tbhelp") != 0) {
    cout<<USAGE<<endl;
    return false;
  }

  sparta_assert(false == datafiles.empty(),
                "Need an output file as the last argument of the test");

  sparta::Scheduler sched;
  Simulator sim(&sched, "mavis_isa_files", "arch/isa_json", datafiles[0],
                input_file);

  if(input_file.length() == 0) {
    sparta::log::MessageSource il(sim.getRoot(),"info","Info Messages");
    il << "No input file specified, exiting gracefully, output not checked";
    return true; //not an error
  }

  cls.populateSimulation(&sim);
  cls.runSimulator(&sim);

  EXPECT_FILES_EQUAL(datafiles[0],
                     "expected_output/" + datafiles[0] + ".EXPECTED");
  return true;
}

int main(int argc, char** argv)
{
  if(!runTest(argc, argv)) return 1;

  REPORT_ERROR;
  return (int)ERROR_CODE;
}
