// ICache test

#include "sparta/sparta.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include "test/core/icache/ICacheSink.hpp"
#include "test/core/icache/ICacheSource.hpp"
#include "test/core/icache/ICacheChecker.hpp"

#include "ICache.hpp"
#include "OlympiaAllocators.hpp"

#include <random>

class ICacheSim : public sparta::app::Simulation{
public:
    ICacheSim(sparta::Scheduler *sched) :
        sparta::app::Simulation("Test_special_params", sched){}

    virtual ~ICacheSim(){
        getRoot()->enterTeardown();
    }
private:
    void buildTree_() override
    {
        auto rtn = getRoot();

        allocators_tn_.reset(new olympia::OlympiaAllocators(rtn));

        // ICache
        tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(rtn,
                                                                "icache",
                                                                sparta::TreeNode::GROUP_NAME_NONE,
                                                                sparta::TreeNode::GROUP_IDX_NONE,
                                                                "Instruction Cache",
                                                                &icache_fact));

        // Source
        tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(rtn,
                                                            "source",
                                                            sparta::TreeNode::GROUP_NAME_NONE,
                                                            sparta::TreeNode::GROUP_IDX_NONE,
                                                            "Source",
                                                            &source_fact));
        // Sink
        tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(rtn,
                                                            "sink",
                                                            sparta::TreeNode::GROUP_NAME_NONE,
                                                            sparta::TreeNode::GROUP_IDX_NONE,
                                                            "Sink",
                                                            &sink_fact));

        tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(rtn,
                                                            "checker",
                                                            sparta::TreeNode::GROUP_NAME_NONE,
                                                            sparta::TreeNode::GROUP_IDX_NONE,
                                                            "Checker",
                                                            &checker_fact));

    };
    void configureTree_() override{};
    void bindTree_() override
    {
        auto * root_node = getRoot();
        // Bind up source
        sparta::bind(root_node->getChildAs<sparta::Port>("icache.ports.in_fetch_req"),
                    root_node->getChildAs<sparta::Port>("source.ports.out_icache_req"));
        sparta::bind(root_node->getChildAs<sparta::Port>("icache.ports.out_fetch_credit"),
                    root_node->getChildAs<sparta::Port>("source.ports.in_icache_credit"));
        sparta::bind(root_node->getChildAs<sparta::Port>("icache.ports.out_fetch_resp"),
                    root_node->getChildAs<sparta::Port>("source.ports.in_icache_resp"));

        // Bind up sink
        sparta::bind(root_node->getChildAs<sparta::Port>("icache.ports.out_l2cache_req"),
                    root_node->getChildAs<sparta::Port>("sink.ports.in_icache_req"));
        sparta::bind(root_node->getChildAs<sparta::Port>("icache.ports.in_l2cache_resp"),
                    root_node->getChildAs<sparta::Port>("sink.ports.out_icache_resp"));
        sparta::bind(root_node->getChildAs<sparta::Port>("icache.ports.in_l2cache_ack"),
                    root_node->getChildAs<sparta::Port>("sink.ports.out_icache_credit"));

        // Bind up checker
        sparta::bind(root_node->getChildAs<sparta::Port>("source.ports.out_icache_req"),
                    root_node->getChildAs<sparta::Port>("checker.ports.in_fetch_req"));
        sparta::bind(root_node->getChildAs<sparta::Port>("icache.ports.out_fetch_resp"),
                    root_node->getChildAs<sparta::Port>("checker.ports.in_fetch_resp"));
        sparta::bind(root_node->getChildAs<sparta::Port>("icache.ports.out_l2cache_req"),
                    root_node->getChildAs<sparta::Port>("checker.ports.in_l2cache_req"));
        sparta::bind(root_node->getChildAs<sparta::Port>("sink.ports.out_icache_resp"),
                    root_node->getChildAs<sparta::Port>("checker.ports.in_l2cache_resp"));
    };

    std::unique_ptr<olympia::OlympiaAllocators> allocators_tn_;

    sparta::ResourceFactory<olympia::ICache, olympia::ICache::ICacheParameterSet> icache_fact;
    sparta::ResourceFactory<icache_test::ICacheSource, icache_test::ICacheSource::ICacheSourceParameters> source_fact;
    sparta::ResourceFactory<icache_test::ICacheSink, icache_test::ICacheSink::ICacheSinkParameters> sink_fact;
    sparta::ResourceFactory<icache_test::ICacheChecker, icache_test::ICacheChecker::ICacheCheckerParameters> checker_fact;

    std::vector<std::unique_ptr<sparta::TreeNode>> tns_to_delete_;

};

const char USAGE[] =
    "Usage:\n"
    "    \n"
    "\n";

sparta::app::DefaultValues DEFAULTS;

int main(int argc, char **argv)
{

    std::string testname;
    uint32_t seed = 1;

    sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
    auto & app_opts = cls.getApplicationOptions();
    app_opts.add_options()
        ("testname",
         sparta::app::named_value<std::string>("TESTNAME", &testname)->default_value(""),
         "Provide a testname to run",
         "Test to run")
        ("seed",
         sparta::app::named_value<uint32_t>("SEED", &seed)->default_value(1),
         "Provide a value to seed the random generators",
         "random seed");

    int err_code = 0;
    if(!cls.parse(argc, argv, err_code)){
        sparta_assert(false, "Command line parsing failed"); // Any errors already printed to cerr
    }

    sparta::Scheduler sched;
    ICacheSim sim(&sched);
    cls.populateSimulation(&sim);
    sparta::RootTreeNode* root = sim.getRoot();

    icache_test::ICacheChecker *checker = root->getChild("checker")->getResourceAs<icache_test::ICacheChecker*>();
    checker->setDUT(root->getChild("icache")->getResourceAs<olympia::ICache*>());

    icache_test::ICacheSource *source = root->getChild("source")->getResourceAs<icache_test::ICacheSource*>();

    icache_test::ICacheSink *sink = root->getChild("sink")->getResourceAs<icache_test::ICacheSink*>();
    sink->setRandomSeed(seed);

    if (testname == "single_access") {
        source->queueRequest(1);
        cls.runSimulator(&sim, 100);
    }
    else if (testname == "simple") {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 8; j++) {
                source->queueRequest(8 << j);
            }
        }
        cls.runSimulator(&sim, 1000);
    }
    else if (testname == "random") {
        std::mt19937 gen(seed);
        std::lognormal_distribution<> d(2.0, 1.0);
        std::vector<uint64_t> addrs = {1};
        for (int i = 0; i < 256; ++i) {
            auto addr = addrs.back() + uint64_t(d(gen));
            addrs.push_back(addr);
        }
        for (int i = 0; i < 2048; ++i) {
            auto idx = uint32_t(d(gen)) % addrs.size();
            source->queueRequest(addrs[idx]);
            if (i % 128 == 0) {
                std::shuffle(addrs.begin(), addrs.end(), gen);
            }
        }
        cls.runSimulator(&sim, 100000);
    }
    else {
        sparta_assert(false, "Must provide a valid testname");
    }
    return 0;
}