
#include "DCache.hpp"
#include <sparta/app/SimulationConfiguration.hpp>
#include <sparta/app/CommandLineSimulator.hpp>
#include "sparta/utils/SpartaTester.hpp"
#include "test/core/dcache/NextLvlSourceSinkUnit.hpp"
#include "test/core/dcache/SourceUnit.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "OlympiaAllocators.hpp"

class DCacheSim : public sparta::app::Simulation
{
  public:
    DCacheSim(sparta::Scheduler* sched, const std::string & mavis_isa_files,
              const std::string & mavis_uarch_files, const std::string & output_file,
              const std::string & input_file) :
        sparta::app::Simulation("DCacheSim", sched),
        input_file_(input_file),
        test_tap_(getRoot(), "info", output_file)
    {
    }

    ~DCacheSim() { getRoot()->enterTeardown(); }

    void runRaw(uint64_t run_time) override final
    {
        (void)run_time;

        sparta::app::Simulation::runRaw(run_time);
    }

  private:
    void buildTree_() override
    {
        auto rtn = getRoot();

        allocators_tn_.reset(new olympia::OlympiaAllocators(rtn));

        sparta::ResourceTreeNode* mavis = new sparta::ResourceTreeNode(
            rtn, olympia::MavisUnit::name, sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE, "Mavis Unit", &mavis_fact);
        tns_to_delete_.emplace_back(mavis);

        // Create a Source Units that represents DCache and ICache
        sparta::ResourceTreeNode* Test_Lsu =
            new sparta::ResourceTreeNode(rtn, "lsu", sparta::TreeNode::GROUP_NAME_NONE,
                                         sparta::TreeNode::GROUP_IDX_NONE, "lsu", &lsu_fact);

        Test_Lsu->getParameterSet()->getParameter("input_file")->setValueFromString(input_file_);
        tns_to_delete_.emplace_back(Test_Lsu);

        sparta::ResourceTreeNode* Test_DCache =
            new sparta::ResourceTreeNode(rtn, "dcache", sparta::TreeNode::GROUP_NAME_NONE,
                                         sparta::TreeNode::GROUP_IDX_NONE, "dcache", &dcache_fact);
        tns_to_delete_.emplace_back(Test_DCache);

        sparta::ResourceTreeNode* Test_Next_Lvl = new sparta::ResourceTreeNode(
            rtn, "next_lvl", sparta::TreeNode::GROUP_NAME_NONE, sparta::TreeNode::GROUP_IDX_NONE,
            "next_lvl", &next_lvl_fact);
        tns_to_delete_.emplace_back(Test_Next_Lvl);
    }

    void configureTree_() override {}

    void bindTree_() override
    {
        auto* root_node = getRoot();

        sparta::bind(root_node->getChildAs<sparta::Port>("lsu.ports.out_source_req"),
                     root_node->getChildAs<sparta::Port>("dcache.ports.in_lsu_lookup_req"));
        sparta::bind(root_node->getChildAs<sparta::Port>("lsu.ports.in_source_resp"),
                     root_node->getChildAs<sparta::Port>("dcache.ports.out_lsu_lookup_req"));
        sparta::bind(root_node->getChildAs<sparta::Port>("lsu.ports.in_source_ack"),
                     root_node->getChildAs<sparta::Port>("dcache.ports.out_lsu_lookup_ack"));

        sparta::bind(root_node->getChildAs<sparta::Port>("next_lvl.ports.in_biu_req"),
                     root_node->getChildAs<sparta::Port>("dcache.ports.out_l2cache_req"));
        sparta::bind(root_node->getChildAs<sparta::Port>("next_lvl.ports.out_biu_resp"),
                     root_node->getChildAs<sparta::Port>("dcache.ports.in_l2cache_resp"));
        sparta::bind(root_node->getChildAs<sparta::Port>("next_lvl.ports.out_biu_ack"),
                     root_node->getChildAs<sparta::Port>("dcache.ports.in_l2cache_credits"));
    }

    std::unique_ptr<olympia::OlympiaAllocators> allocators_tn_;

    sparta::ResourceFactory<dcache_test::SourceUnit, dcache_test::SourceUnit::SourceUnitParameters>
        lsu_fact;

    sparta::ResourceFactory<olympia::DCache, olympia::DCache::CacheParameterSet> dcache_fact;

    sparta::ResourceFactory<dcache_test::NextLvlSourceSinkUnit,
                            dcache_test::NextLvlSourceSinkUnit::NextLvlSinkUnitParameters>
        next_lvl_fact;

    olympia::MavisFactory mavis_fact;
    std::vector<std::unique_ptr<sparta::TreeNode>> tns_to_delete_;

    const std::string input_file_;
    sparta::log::Tap test_tap_;
};

const char USAGE[] = "Usage:\n"
                     "    \n"
                     "\n";
sparta::app::DefaultValues DEFAULTS;

void runTest(int argc, char** argv)
{
    DEFAULTS.auto_summary_default = "off";
    std::vector<std::string> datafiles;
    std::string input_file;

    sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
    auto & app_opts = cls.getApplicationOptions();
    app_opts.add_options()(
        "output_file",
        sparta::app::named_value<std::vector<std::string>>("output_file", &datafiles),
        "Specifies the output file")(
        "input-file",
        sparta::app::named_value<std::string>("INPUT_FILE", &input_file)->default_value(""),
        "Provide a JSON instruction stream",
        "Provide a JSON file with instructions to run through Execute");

    po::positional_options_description & pos_opts = cls.getPositionalOptions();
    pos_opts.add("output_file", -1); // example, look for the <data file> at the end

    int err_code = 0;
    if (!cls.parse(argc, argv, err_code))
    {
        sparta_assert(false, "Command line parsing failed"); // Any errors already printed to cerr
    }

    sparta_assert(false == datafiles.empty(),
                  "Need an output file as the last argument of the test");

    sparta::Scheduler sched;
    DCacheSim dcache_sim(&sched, "mavis_isa_files", "arches/isa_json", datafiles[0], input_file);

    cls.populateSimulation(&dcache_sim);

    cls.runSimulator(&dcache_sim);

    EXPECT_FILES_EQUAL(datafiles[0], "expected_output/" + datafiles[0] + ".EXPECTED");
}

int main(int argc, char** argv)
{
    runTest(argc, argv);
    REPORT_ERROR;
    return (int)ERROR_CODE;
}
