#include <boost/program_options.hpp>
#include <vector>
#include <string>
#include <iostream>

#include "sparta/simulation/TreeNode.hpp"

#include "core/MavisUnit.hpp"
#include "sim/OlympiaAllocators.hpp"
#include "mavis/Mavis.h"

int main(int argc, char **argv)
{
    namespace po = boost::program_options;
    po::options_description desc("dump_inst -- a program that details how olympia sees a given instruction");
    desc.add_options()
        ("help,h", "Command line options")
        ("opc,o",  po::value<std::vector<std::string>>(), "32-bit or 16-bit hex opcode")
        ("mnemonic,m", po::value<std::vector<std::string>>(), "Mnemonic to look up");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if(vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    // Dummy node
    sparta::TreeNode rtn("mavis_tree", "Mavis Tree node");

    // Create the allocators used by Mavis
    olympia::OlympiaAllocators allocators(&rtn);

    // Create the Mavis Unit and obtain the facade
    olympia::MavisUnit::MavisParameters mavis_params(&rtn);
    olympia::MavisUnit mavis_unit(&rtn, &mavis_params);

    const auto mavis_facade = mavis_unit.getFacade();
    sparta_assert(nullptr != mavis_facade);

    if(vm.count("opc")) {
        for(auto opcode : vm["opc"].as<std::vector<std::string>>()) {
            auto inst = mavis_facade->makeInst(std::stol(opcode, 0, 16));
            if(nullptr != inst) {
                std::cout << inst << std::endl;
            }
        }
    }

    if(vm.count("mnemonic")) {
        for(auto mnemonic : vm["mnemonic"].as<std::vector<std::string>>())
        {
            mavis::ExtractorDirectInfo ex_data(mnemonic,
                                               olympia::MavisType::RegListType(),
                                               olympia::MavisType::RegListType());
            auto inst = mavis_facade->makeInstDirectly(ex_data);
            if(nullptr != inst) {
                std::cout << inst << std::endl;
            }
        }
    }

    return 0;
}
