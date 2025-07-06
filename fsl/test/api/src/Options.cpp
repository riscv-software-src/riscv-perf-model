// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh, Condor Computing Corp.
//
#include "Msg.h"
#include "Options.h"
#include <iostream>
using namespace std;

// --------------------------------------------------------------------
// Build the option set and check the options
// --------------------------------------------------------------------
void Options::setupOptions(int ac, char** av)
{
    notify_error = false;

    po::options_description visibleOpts(
        "\nFusion API test\n "
        "Usage:: test [--help|-h|--version|-v] { options }");

    po::options_description stdOpts("Standard options");
    buildOptions(stdOpts);

    try
    {
        po::store(po::command_line_parser(ac, av).options(stdOpts).run(),
                  vm);

        // Without positional option po::parse_command_line can be used
        // po::store(po::parse_command_line(ac, av, allOpts), vm);
    }
    catch (boost::program_options::error & e)
    {
        msg->msg("");
        msg->emsg("1st pass command line option parsing failed");
        msg->emsg("What: " + string(e.what()));
        usage(stdOpts);
        exit(1);
    }

    po::notify(vm);
    if (!checkOptions(vm, stdOpts, true))
        exit(1);
}

// --------------------------------------------------------------------
// Construct the std, hidden and positional option descriptions
// --------------------------------------------------------------------
// clang-format off
void Options::buildOptions(po::options_description & stdOpts)
{
    stdOpts.add_options()

    ("help,h", "...")

    ("version,v", "report version and exit")

    ("stf", po::value<string>(&stf_file), "STF input file")

    ("output,o", po::value<string>(&output_file),
     "Log output file")

    ("isa_file", po::value<vector<string>>(&isa_files),
     "Multiple --isa_file accepted")

    ("fsl_file", po::value<vector<string>>(&fsl_files),
     "Multiple --fsl_file accepted")

    ("fsl_syntax_file", po::value<vector<string>>(&fsl_syntax_files),
     "Syntax stress test files. Multiple "
     "--fsl_syntax_file accepted")

    ("tb_verbose", po::bool_switch(&tb_verbose) ->default_value(false),
         "Test bench message control");
}
// clang-format on
// --------------------------------------------------------------------
// Check sanity on the options, handle --help, --version
// --------------------------------------------------------------------
bool Options::checkOptions(po::variables_map & vm,
                           po::options_description & stdOpts,
                           bool firstPass)
{
    if (firstPass)
    {
        if (vm.count("help"))
        {
            usage(stdOpts);
            return false;
        }
        if (vm.count("version"))
        {
            version();
            return false;
        }
    }

    return true;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
void Options::version() const
{
    msg->imsg("");
    msg->imsg("Fusion api tester");
    msg->imsg("Slack jeff w/any questions");
    msg->imsg("");
}
