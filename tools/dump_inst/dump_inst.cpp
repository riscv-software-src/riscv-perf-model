#include <boost/program_options.hpp>
#include <vector>
#include <string>
#include <iostream>

#include "sparta/utils/StringUtils.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/LogUtils.hpp"

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
        ("pair,p",  po::value<std::vector<std::string>>(),
         "32-bit or 16-bit hex opcode pairs, comma separated."
         " Compare opcodes for overlaps")
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
            else {
                std::cerr << "ERROR: " << HEX8(opcode) << " is not decodable" << std::endl;
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
            else {
                std::cerr << "ERROR: " << mnemonic << " is not decodable" << std::endl;
            }
        }
    }

    if(vm.count("pair")) {

        for(auto pairs : vm["pair"].as<std::vector<std::string>>())
        {
            std::vector<std::string> opc_pairs_str;
            sparta::utils::tokenize(pairs.c_str(), opc_pairs_str, ",");
            if(opc_pairs_str.size() != 2) {
                std::cerr << "ERROR: " << opc_pairs_str << " is not a comma separated list of opcodes" << std::endl;
                return(-1);
            }
            const auto opc1 = std::stol(opc_pairs_str[0], 0, 16);
            const auto opc2 = std::stol(opc_pairs_str[1], 0, 16);
            const auto inst1 = mavis_facade->makeInst(opc1);
            const auto inst2 = mavis_facade->makeInst(opc2);
            if(nullptr == inst1 || nullptr == inst2) {
                std::cerr << "ERROR: " << HEX8(opc1) << " or " << HEX8(opc2) << " is not decodable" << std::endl;
            }

            const auto & opcode_info_inst1 = inst1->getOpcodeInfo();
            const auto & opcode_info_inst2 = inst1->getOpcodeInfo();
            const auto & dest_op_mask      = opcode_info_inst1->getDestRegs();
            const auto & src_op_mask       = opcode_info_inst2->getSourceRegs();
            const auto overlaps = (dest_op_mask | src_op_mask);
            if(overlaps.any())
            {
                const auto overlap_val = overlaps._Find_first();
                std::cout << "They **might** overlap on register: " << overlap_val << std::endl;

                // const auto & dest_op_list = inst1->getDestOpInfoList();
                // const auto & src_op_list  = inst2->getSourceOpInfoList();

                if(opcode_info_inst1->numIntDestRegs()   != 0 &&
                   opcode_info_inst2->numIntSourceRegs() != 0)
                {
                    std::cout << "Strong match on integer dest -> integer source" << std::endl;
                }
                else {
                    std::cout << "Doubtful match" << std::endl;
                }
            }
        }

    }

    return 0;
}
