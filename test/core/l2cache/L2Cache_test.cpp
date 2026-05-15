
#include "L2Cache.hpp"
#include "decode/MavisUnit.hpp"
#include "utils/CoreUtils.hpp"

#include "test/core/common/SourceUnit.hpp"
#include "test/core/common/SinkUnit.hpp"

#include "test/core/l2cache/BIUSinkUnit.hpp"
#include "test/core/l2cache/L2SourceUnit.hpp"

#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/resources/Buffer.hpp"
#include "sparta/sparta.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "OlympiaAllocators.hpp"

#include <memory>
#include <vector>
#include <cinttypes>
#include <sstream>
#include <initializer_list>

TEST_INIT

//
// Simple L2Cache Simulator.
//
// SourceUnit 0 <-> L2Cache <-> BIUSinkUnit
//                  ^
//                  |
//                  |
// SourceUnit 1 <---
//
class L2CacheSim : public sparta::app::Simulation
{
public:

    L2CacheSim(sparta::Scheduler * sched,
                const std::string & mavis_isa_files,
                const std::string & mavis_uarch_files,
                const std::string & output_file,
                const std::string & input_file) :
        sparta::app::Simulation("L2CacheSim", sched),
        input_file_(input_file),
        test_tap_(getRoot(), "info", output_file)
    {
    }

    ~L2CacheSim() {
        getRoot()->enterTeardown();
    }

    void runRaw(uint64_t run_time) override final
    {
        (void) run_time; // ignored

        sparta::app::Simulation::runRaw(run_time);
    }

private:

    void buildTree_()  override
    {
        auto rtn = getRoot();

        // Cerate the common Allocators
        allocators_tn_.reset(new olympia::OlympiaAllocators(rtn));

         // Create a Mavis Unit
        sparta::ResourceTreeNode * mavis = new sparta::ResourceTreeNode(rtn,
                                                                 olympia::MavisUnit::name,
                                                                 sparta::TreeNode::GROUP_NAME_NONE,
                                                                 sparta::TreeNode::GROUP_IDX_NONE,
                                                                 "Mavis Unit",
                                                                 &mavis_fact);
        tns_to_delete_.emplace_back(mavis);

        // Create a Source Units that represents DCache and ICache
        sparta::ResourceTreeNode * Test_DCache  = new sparta::ResourceTreeNode(rtn,
                                                                            "dcache",
                                                                            sparta::TreeNode::GROUP_NAME_NONE,
                                                                            sparta::TreeNode::GROUP_IDX_NONE,
                                                                            "dcache",
                                                                            &dcache_fact);
        Test_DCache->getParameterSet()->getParameter("input_file")->setValueFromString(input_file_);
        tns_to_delete_.emplace_back(Test_DCache);

        sparta::ResourceTreeNode * Test_ICache = new sparta::ResourceTreeNode(rtn,
                                                                            "icache",
                                                                            sparta::TreeNode::GROUP_NAME_NONE,
                                                                            sparta::TreeNode::GROUP_IDX_NONE,
                                                                            "icache",
                                                                            &icache_fact);
        Test_ICache->getParameterSet()->getParameter("input_file")->setValueFromString(input_file_);
        tns_to_delete_.emplace_back(Test_ICache);

        // Create L2Cache
        sparta::ResourceTreeNode * L2CacheUnit = new sparta::ResourceTreeNode(rtn,
                                                                        "l2cache",
                                                                        sparta::TreeNode::GROUP_NAME_NONE,
                                                                        sparta::TreeNode::GROUP_IDX_NONE,
                                                                        "l2cache",
                                                                        &l2cache_fact);
        tns_to_delete_.emplace_back(L2CacheUnit);

        // Create SinkUnit that represents the BIU
        sparta::ResourceTreeNode * Test_BIU  = new sparta::ResourceTreeNode(rtn,
                                                                       "biu",
                                                                       sparta::TreeNode::GROUP_NAME_NONE,
                                                                       sparta::TreeNode::GROUP_IDX_NONE,
                                                                       "biu",
                                                                       &biu_fact);
        tns_to_delete_.emplace_back(Test_BIU);
    }

    void configureTree_()  override { }

    void bindTree_()  override
    {
        auto * root_node = getRoot();

        // Bind the "ROB" (simple SinkUnit that accepts instruction
        // groups) to Dispatch
        sparta::bind(root_node->getChildAs<sparta::Port>("dcache.ports.out_source_req"),
                     root_node->getChildAs<sparta::Port>("l2cache.ports.in_dcache_l2cache_req"));
        sparta::bind(root_node->getChildAs<sparta::Port>("dcache.ports.in_source_resp"),
                     root_node->getChildAs<sparta::Port>("l2cache.ports.out_l2cache_dcache_resp"));
        sparta::bind(root_node->getChildAs<sparta::Port>("dcache.ports.in_source_credits"),
                     root_node->getChildAs<sparta::Port>("l2cache.ports.out_l2cache_dcache_credits"));

        sparta::bind(root_node->getChildAs<sparta::Port>("icache.ports.out_source_req"),
                     root_node->getChildAs<sparta::Port>("l2cache.ports.in_icache_l2cache_req"));
        sparta::bind(root_node->getChildAs<sparta::Port>("icache.ports.in_source_resp"),
                     root_node->getChildAs<sparta::Port>("l2cache.ports.out_l2cache_icache_resp"));
        sparta::bind(root_node->getChildAs<sparta::Port>("icache.ports.in_source_credits"),
                     root_node->getChildAs<sparta::Port>("l2cache.ports.out_l2cache_icache_credits"));

        sparta::bind(root_node->getChildAs<sparta::Port>("biu.ports.in_biu_req"),
                     root_node->getChildAs<sparta::Port>("l2cache.ports.out_l2cache_biu_req"));
        sparta::bind(root_node->getChildAs<sparta::Port>("biu.ports.out_biu_resp"),
                     root_node->getChildAs<sparta::Port>("l2cache.ports.in_biu_l2cache_resp"));
        sparta::bind(root_node->getChildAs<sparta::Port>("biu.ports.out_biu_credits"),
                     root_node->getChildAs<sparta::Port>("l2cache.ports.in_biu_l2cache_credits"));
    }
    // Allocators.  Last thing to delete
    std::unique_ptr<olympia::OlympiaAllocators> allocators_tn_;

    sparta::ResourceFactory<l2cache_test::L2SourceUnit, l2cache_test::L2SourceUnit::L2SourceUnitParameters> dcache_fact;
    sparta::ResourceFactory<l2cache_test::L2SourceUnit, l2cache_test::L2SourceUnit::L2SourceUnitParameters> icache_fact;

    sparta::ResourceFactory<olympia_mss::L2Cache, olympia_mss::L2Cache::L2CacheParameterSet> l2cache_fact;

    sparta::ResourceFactory<l2cache_test::BIUSinkUnit, l2cache_test::BIUSinkUnit::BIUSinkUnitParameters> biu_fact;

    olympia::MavisFactory            mavis_fact;
    std::vector<std::unique_ptr<sparta::TreeNode>> tns_to_delete_;

    const std::string input_file_;
    sparta::log::Tap test_tap_;
};

const char USAGE[] =
    "Usage:\n"
    "    \n"
    "\n";

sparta::app::DefaultValues DEFAULTS;

// The main tester of L2Cache.  The test is encapsulated in the
// parameter test_type of the Source unit.
void runTest(int argc, char **argv)
{
    DEFAULTS.auto_summary_default = "off";
    std::vector<std::string> datafiles;
    std::string input_file;

    sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
    auto & app_opts = cls.getApplicationOptions();
    app_opts.add_options()
        ("output_file",
         sparta::app::named_value<std::vector<std::string>>("output_file", &datafiles),
         "Specifies the output file")
        ("input-file",
         sparta::app::named_value<std::string>("INPUT_FILE", &input_file)->default_value(""),
         "Provide a JSON instruction stream",
         "Provide a JSON file with instructions to run through Execute");

    po::positional_options_description& pos_opts = cls.getPositionalOptions();
    pos_opts.add("output_file", -1); // example, look for the <data file> at the end

    int err_code = 0;
    if(!cls.parse(argc, argv, err_code)){
        sparta_assert(false, "Command line parsing failed"); // Any errors already printed to cerr
    }

    sparta_assert(false == datafiles.empty(), "Need an output file as the last argument of the test");

    sparta::Scheduler sched;
    L2CacheSim l2cache_sim(&sched, "mavis_isa_files", "arches/isa_json", datafiles[0], input_file);

    cls.populateSimulation(&l2cache_sim);

    cls.runSimulator(&l2cache_sim);

    EXPECT_FILES_EQUAL(datafiles[0], "expected_output/" + datafiles[0] + ".EXPECTED");
}

int main(int argc, char **argv)
{
    runTest(argc, argv);

    REPORT_ERROR;
    return (int)ERROR_CODE;
}
