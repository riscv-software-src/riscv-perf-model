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
    {
        exit(1);
    }
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

    ("output,o", po::value<string>(&output_file),
     "Output file")

    ("input_file,i", po::value<vector<string>>(&input_files),
     "Multiple --input_file accepted")

    ("trace_en", po::bool_switch(&trace_en) ->default_value(false),
         "Parser trace enable")

    ("verbose", po::bool_switch(&verbose) ->default_value(false),
         "Verbose message control")
    ;
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
        if (vm.count("help") != 0u)
        {
            usage(stdOpts);
            return false;
        }
        if (vm.count("version") != 0u)
        {
            version();
            return false;
        }
    }

    return true;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
void Options::version()
{
    msg->imsg("");
    msg->imsg("Fusion api tester");
    msg->imsg("Slack jeff w/any questions");
    msg->imsg("");
}
