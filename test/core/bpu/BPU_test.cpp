// BPU test

#include "sparta/sparta.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <string>

class BPUSim : public class sparta::app::Simulation
{
    public:
        BPUSim(sparta::Scheduler *sched) : sparta::app::Simulatiuon("BPUSim", sched) 
        {}

        virtual ~ICacheSim(){
            getRoot()->enterTeardown();
        }

    private:
        void buildTree_() override {
            continue;
        }

        void configureTree_() override {};

        void bindTree_() override {
            continue;
        }
};

int main(int argc, char **argv) {
    std::string testname;

    sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);
    auto & app_opts = cls.getApplicationOptions();
    app_opts.add_options()
        ("testname",
         sparta::app::named_value<std::string>("TESTNAME", &testname)->default_value(""),
         "Provide a testname to run",
         "Test to run");

    int err_code = 0;
    if(!cls.parse(argc, argv, err_code)){
        sparta_assert(false, "Command line parsing failed"); // Any errors already printed to cerr
    }

    sparta::Scheduler sched;
    BPUSim sim(&sched);
    cls.populateSimulation(&sim);
    sparta::RootTreeNode* root = sim.getRoot();
}