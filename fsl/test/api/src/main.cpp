// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh, Condor Computing Corp.
//
#include "fsl_interp/inc/FslParser.h"
#include "ApiTestBench.h"
#include "Msg.h"
#include "Options.h"

#include <fstream>
#include <iostream>
using namespace std;

FslParser* FP;

Options* Options::instance = 0;
std::shared_ptr<Options> opts(Options::getInstance());

Msg* Msg::instance = 0;
std::unique_ptr<Msg> msg(Msg::getInstance());

//  ------------------------------------------------------------------------
int main(int ac, char** av)
{
    FslParser fp;
    FP = &fp;

    TestBench tb(ac, av);
    ofstream out("PASSFAIL");
    if (!out.is_open())
    {
        msg->emsg("Could not open pass/fail status file");
        return 1;
    }

    msg->imsg("Test run begin");
    bool ok = tb.run();
    if (!ok)
    {
        out << "FAIL" << endl;
        msg->emsg("Test run end  FAIL");
        return 1;
    }
    out << "PASS" << endl;
    msg->imsg("Test run end  PASS");
    return 0;
}
