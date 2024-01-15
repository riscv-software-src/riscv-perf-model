#include "fieldextractor.h"
#include "fusion.h"
#include "fusioncontext.h"
#include "fusiongroup.h"
#include "fusiontypes.h"
#include "machineinfo.h"
#include "msg.h"
#include "options.h"
#include "radixtrie.h"
#include "testbench.h"

#include <boost/range/combine.hpp>

#include <filesystem>
#include <unordered_map>
#include <vector>
using namespace std;

// ====================================================================
// ====================================================================
TestBench::TestBench(int ac, char** av)
{
    msg->setWho("TestBench");
    opts->setup_options(ac, av);
    verbose = opts->tb_verbose;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
bool TestBench::run()
{
    // sanity check files
    if (!sanity_test())
        return false;
    if (!basic_mavis_test())
        return false;
    if (!basic_constraints_test())
        return false;
    if (!fusiongroup_alt_ctor_test())
        return false;
    if (!fusiongroup_cfg_ctor_test())
        return false;
    if (!fusion_context_test(true))
        return false;

    if (!fusion_ctor_compile_test(true))
        return false;
    if (!fusion_search_test(true))
        return false;

    // FieldExtractor method tests
    if (!field_extractor_tests(true))
        return false;

    // domain language tests
    if (!fsl_tests(true))
        return false;

    return true;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
bool TestBench::fusion_context_test(bool debug)
{
    if (verbose)
        msg->imsg("fusion_context_test BEGIN");

    using FusionGroupType =
        fusion::FusionGroup<MachineInfo, FieldExtractor>;
    using FusionGroupListType = std::vector<FusionGroupType>;

    fusion::FusionContext<FusionGroupType> context_;

    FusionGroupType grp1("uf1", uf1, cb_proxy::uf1_func);
    FusionGroupType grp2("uf2", uf2, cb_proxy::uf2_func);
    FusionGroupType grp3("uf3", uf3, cb_proxy::uf3_func);

    fusion::HashType grp1_hash = grp1.hash();
    fusion::HashType grp2_hash = grp2.hash();
    fusion::HashType grp3_hash = grp3.hash();

    FusionGroupListType grouplist = {grp1, grp2, grp3};

    try
    {
        context_.makeContext("fusion_context_test", grouplist);
    }
    catch (const fusion::ContextDuplicateError & e)
    {
        std::cerr << "Caught ContextDuplicateError: " << e.what() << endl;
    }
    catch (const std::exception & e)
    {
        std::cerr << "Caught unclassified exception: " << e.what() << endl;
        return false;
    }

    // Future:  fusion::RadixTree *rtrie = context_.getTree();

    if (verbose)
        msg->imsg("fusion_context_test END");
    return true;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
bool TestBench::fusion_search_test(bool debug)
{
    if (verbose)
        msg->imsg("fusion_search_test BEGIN");
    bool ok = true;

    using FusionGroupType =
        fusion::FusionGroup<MachineInfo, FieldExtractor>;
    using FusionGroupCfgType =
        fusion::FusionGroupCfg<MachineInfo, FieldExtractor>;
    using FusionType =
        fusion::Fusion<FusionGroupType, MachineInfo, FieldExtractor>;

    FusionType::FusionGroupCfgListType test_cases = {
        FusionGroupCfgType{.name = {"UF1"},
                           .uids = uf1,
                           .transformFunc =
                               &TestBench::cb_proxy::uf1_func},
        FusionGroupCfgType{.name = {"UF1_1"},
                           .uids = uf1_1,
                           .transformFunc =
                               &TestBench::cb_proxy::uf1_1_func},
        FusionGroupCfgType{.name = {"UF1_2"},
                           .uids = uf1_2,
                           .transformFunc =
                               &TestBench::cb_proxy::uf1_2_func},
        FusionGroupCfgType{.name = {"UF1_3"},
                           .uids = uf1_3,
                           .transformFunc =
                               &TestBench::cb_proxy::uf1_3_func},
        FusionGroupCfgType{.name = {"UF2"},
                           .uids = uf2,
                           .transformFunc =
                               &TestBench::cb_proxy::uf2_func},
        FusionGroupCfgType{.name = {"UF3"},
                           .uids = uf3,
                           .transformFunc =
                               &TestBench::cb_proxy::uf3_func}};

    // Future add specific tests for hash creation
    //   std::unordered_map<string,fusion::HashType> expHashes;
    //   generateExpectHashes(expHashes,test_cases);

    InstPtrListType in, out;

    assign(in, of1, opts->isa_files);
    assign(out, of1, opts->isa_files);

    size_t outSize = out.size();
    size_t inSize = in.size();

    FusionType f(test_cases);
    f.fusionOperator(in, out);

    // The default operator appends in to out and clears in
    if (in.size() != 0)
    {
        msg->emsg("fusionOperator failed to clean input vector");
        ok = false;
    }

    if (out.size() != (outSize + inSize))
    {
        msg->emsg(
            "fusionOperator failed to properly append to output vector");
        ok = false;
    }

    // Test the custom operator as lambda
    auto customLambda = [](FusionType & inst, fusion::InstPtrListType & in,
                           fusion::InstPtrListType & out)
    {
        out = in; // in is not cleared
    };

    // Restore in/out
    in.clear();
    out.clear();
    InstPtrListType tmp;
    assign(in, of1, opts->isa_files);
    inSize = in.size();

    f.setFusionOpr(customLambda);
    f.fusionOperator(in, out);

    // Did the lambda clear the in vector
    if (in.size() == 0)
    {
        msg->emsg("the custom fusionOperator incorrectly cleared the "
                  "input vector");
        ok = false;
    }

    // The resulting out vector should be the same size as in and tmp
    bool sameSize = inSize == out.size() && inSize == in.size();
    if (!sameSize)
    {
        msg->emsg("with the custom fusionOperator the vector sizes are "
                  "mismatched");
        ok = false;
    }

    if (verbose)
        msg->imsg("fusion_search_test END");
    return ok;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
void TestBench::generateExpectHashes(
    unordered_map<string, fusion::HashType> & exp,
    const FusionType::FusionGroupCfgListType & in)
{
    using FBASE = fusion::FusionGroupBase;
    for (size_t i = 0; i < in.size(); ++i)
    {
        FusionGroupCfgType cfg = in[i];
        fusion::HashType expHash = jenkins_1aat(cfg.uids.value());
        exp.insert(make_pair(cfg.name, expHash));
    }
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
bool TestBench::fusion_ctor_compile_test(bool debug)
{
    if (verbose)
        msg->imsg("fusion_ctor_compile_test BEGIN");
    bool ok = true;

    using FusionGroupType =
        fusion::FusionGroup<MachineInfo, FieldExtractor>;
    using FusionType =
        fusion::Fusion<FusionGroupType, MachineInfo, FieldExtractor>;

    // compile checks
    FusionType f1;
    FusionType f2{};
    FusionType f3 =
        fusion::Fusion<FusionGroupType, MachineInfo, FieldExtractor>();

    const FusionType::FusionGroupListType fusiongroup_list = {};
    const FusionType::FusionGroupCfgListType fusiongroup_cfg_list = {};
    const fusion::FileNameListType txt_file_list = {};

    FusionType f4(fusiongroup_list);
    FusionType f5(fusiongroup_cfg_list);
    FusionType f6(txt_file_list);

    if (verbose)
        msg->imsg("fusion_ctor_compile_test END");
    return ok;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
bool TestBench::basic_mavis_test(bool debug)
{
    if (verbose)
        msg->imsg("basic_mavis_test BEGIN");
    InstUidListType goldenUid = uf1; // { 0xb,   0xd,  0x1c,  0xf,   0x13 };
    OpcodeListType goldenOpc  = of1; // { 0x76e9,0x685,0x8d35,0x1542,0x9141 };

    Instruction<uArchInfo>::PtrType inst = nullptr;

    using MavisType = Mavis<Instruction<uArchInfo>, uArchInfo>;
    MavisType mavis_facade(opts->isa_files, {});

    // Make sure the opcodes convert correctly
    InstPtrListType instrs;
    for (const auto opc : goldenOpc)
    {
        try
        {
            instrs.emplace_back(mavis_facade.makeInst(opc, 0));
        }
        catch (const mavis::IllegalOpcode & ex)
        {
            cout << ex.what() << endl;
            msg->emsg("basic_mavis_test failed to convert opcode");
            return false;
        }
    }

    InstUidListType uids;
    for (const auto inst : instrs)
    {
        uids.emplace_back(inst->getUID());
    }

    if (instrs.size() != goldenUid.size()
        || instrs.size() != goldenOpc.size()
        || instrs.size() != uids.size())
    {
        msg->emsg("basic_mavis_test size mismatch in inst vector");
        return false;
    }

    // FIXME: There is an unexplained difference in UID creation
    //     if (goldenUid != uids)
    //     {
    //         msg->emsg("basic_mavis_test inst to uid  conversion
    //         failed"); return false;
    //     }

    if (debug)
        info(goldenUid, uids, instrs);

    if (verbose)
        msg->imsg("basic_mavis_test END");
    return true;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
bool TestBench::fusiongroup_alt_ctor_test()
{
    if (verbose)
        msg->imsg("fusiongroup_alt_ctor_test BEGIN");

    // fusion ctor compile checks
    fusion::FusionGroup<MachineInfo, FieldExtractor> f1;
    fusion::FusionGroup<MachineInfo, FieldExtractor> f2{};
    fusion::FusionGroup<MachineInfo, FieldExtractor> f3 =
        fusion::FusionGroup<MachineInfo, FieldExtractor>();

    struct OtherMachine
    {
        OtherMachine() {}
    };

    struct OtherExtractor
    {
        OtherExtractor() {}
    };

    // Alternative machineinfo and extractor
    using AltFusionGroupType =
        fusion::FusionGroup<OtherMachine, OtherExtractor>;

    InstUidListType alt_uid = {};

    AltFusionGroupType alt1("alt1");
    AltFusionGroupType alt2("alt2", alt_uid);

    struct proxy
    {
        static bool alt_func(AltFusionGroupType &, InstPtrListType &,
                             InstPtrListType &)
        {
            return true;
        }
    };

    bool ok = true;

    AltFusionGroupType alt3("alt3", alt_uid);
    alt3.setTransform(proxy::alt_func);

    InstPtrListType in, out;

    if (!alt3.transform(in, out))
    {
        msg->emsg("alt3.transform() failed");
        ok = false;
    }

    AltFusionGroupType alt4("alt4", alt_uid, proxy::alt_func);
    if (!alt4.transform(in, out))
    {
        msg->emsg("alt4.transform() failed");
        ok = false;
    }

    if (verbose)
        msg->imsg("fusiongroup_alt_ctor_test END");
    return ok;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
bool TestBench::fusiongroup_cfg_ctor_test()
{
    if (verbose)
        msg->imsg("fusiongroup_cfg_ctor_test BEGIN");
    using FusionGroupCfgType =
        fusion::FusionGroupCfg<MachineInfo, FieldExtractor>;

    // ---------------------------------------------------------------
    // Test that hash created from the F1CfgUid matches the hash from a
    // base class reference instance
    // ---------------------------------------------------------------
    // reference version
    fusion::FusionGroupBase reference;
    reference.setUids(uf1);
    fusion::HashType referenceHash = reference.hash();

    // With uids, no opcs, no mavis
    FusionGroupCfgType F1CfgUid{.name = {"F1CfgUid"},
                                .uids = uf1,
                                .transformFunc = &f1_constraints};

    bool ok = true;

    using FusionGroupType =
        fusion::FusionGroup<MachineInfo, FieldExtractor>;
    FusionGroupType F1fromF1CfgUid(F1CfgUid);

    F1fromF1CfgUid.info();

    if (referenceHash != F1fromF1CfgUid.hash())
    {
        msg->emsg("F1fromF1CfgUid hash does not match reference hash");
        ok = false;
    }
    // ---------------------------------------------------------------
    // Test that the F1CfgUid ctor results in a FusionGroup that can
    // correctly transform this input group
    InstPtrListType in, out;
    // assign the input vector before transform
    assign(in, of1, opts->isa_files);

    if (!F1fromF1CfgUid.transform(in, out))
    {
        msg->emsg("F1fromF1CfgUid.transform() returned false");
        ok = false;
    }

    if (in.size() != 0)
    {
        msg->emsg("F1fromF1CfgUid.f1_constraints failed to modify input");
        ok = false;
    }
    if (out.size() != 1)
    {
        msg->emsg("F1fromF1CfgUid.f1_constraints failed to modify output");
        ok = false;
    }
    // ---------------------------------------------------------------
    // Test that a FusionGroupCfg constructed from opcodes acts like
    // a FusionGroupCfg constructed from respective UIDs
    //
    // Future support

    // ---------------------------------------------------------------
    // Test that a FusionGroupCfg constructed from assembly statements acts
    // like a FusionGroupCfg constructed from respective UIDs
    //
    // Future support

    if (verbose)
        msg->imsg("fusiongroup_cfg_ctor_test END");
    return ok;
}

// --------------------------------------------------------------------
// Fusion group transform test
// --------------------------------------------------------------------
bool TestBench::basic_constraints_test()
{
    if (verbose)
        msg->imsg("basic_constraints_test BEGIN");

    FusionGroupType F1("F1", TestBench::uf1, f1_constraints);

    InstPtrListType in;
    InstPtrListType out;

    // Create instr from opcodes
    assign(in, of1, opts->isa_files);

    bool ok = true;

    if (!F1.transform(in, out))
    {
        msg->emsg("F1.transform() returned false");
        ok = false;
    }

    if (in.size() != 0)
    {
        msg->emsg("F1.f1_constraints failed to modify input");
        ok = false;
    }

    if (out.size() != 1)
    {
        msg->emsg("F1.f1_constraints failed to modify output");
        ok = false;
    }

    FusionGroupType F2("F2", TestBench::uf2);

    if (F2.getTransform() != nullptr)
    {
        msg->emsg("F2.transform() was not a nullptr as expected");
        ok = false;
    }

    F2.setTransform(f1_constraints);

    if (F2.getTransform() == nullptr)
    {
        msg->emsg("F2.transform() was not set to handler as expected");
        ok = false;
    }

    if (F2.transform(in, out))
    {
        msg->emsg("F2.transform() failed to reject uf2 sequence");
        ok = false;
    }

    if (verbose)
        msg->imsg("basic_constraints_test END");
    return ok;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
bool TestBench::radix_trie_test(bool)
{
    if (verbose)
        msg->imsg("radix_trie_test BEGIN");
    RadixTrie<4> trie;
    uint32_t num_values = 1024 * 1024;

    bool ok = true;

    std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<uint32_t> distribution(
        0, std::numeric_limits<uint32_t>::max());

    auto start = std::chrono::high_resolution_clock::now();
    for (uint32_t i = 0; i < num_values; ++i)
    {
        uint32_t randomNumber = distribution(generator);
        trie.insert(randomNumber);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> insertDuration = end - start;
    std::cout << "Time taken for insertion: " << insertDuration.count()
              << " seconds" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    for (uint32_t i = 0; i < num_values; ++i)
    {
        uint32_t randomNumber = distribution(generator);
        trie.search(randomNumber);
    }

    end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> searchDuration = end - start;
    std::cout << "Time taken for searching: " << searchDuration.count()
              << " seconds" << std::endl;

    trie.insert(12345);
    trie.insert(67890);

    std::cout << "Found '12345' " << (trie.search(12345) ? "Yes" : "No")
              << std::endl;
    std::cout << "Found '67890' " << (trie.search(67890) ? "Yes" : "No")
              << std::endl;
    std::cout << "Found '54321' " << (trie.search(54321) ? "Yes" : "No")
              << std::endl;

    if (trie.search(12345) && trie.search(67890) && !trie.search(54321))
    {
        ok = true;
    }
    else
    {
        ok = false;
    }
    if (verbose)
        msg->imsg("radix_trie_test END");
    return ok;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
bool TestBench::sanity_test(bool)
{
    bool ok = true;

    for (auto fn : opts->isa_files)
    {
        if (!std::filesystem::exists(fn))
        {
            msg->emsg("Can not find isa file " + fn);
            ok = false;
        }
    }

    return ok;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
void TestBench::assign(InstPtrListType & in, OpcodeListType & opcodes,
                       const FileNameListType & json_files)
{
    using MavisType = Mavis<Instruction<uArchInfo>, uArchInfo>;
    MavisType mavis_facade(json_files, {});

    Instruction<uArchInfo>::PtrType inst = nullptr;

    for (const auto icode : opcodes)
    {
        try
        {
            in.emplace_back(mavis_facade.makeInst(icode, 0));
        }
        catch (const mavis::IllegalOpcode & ex)
        {
            cout << ex.what() << endl;
        }
    }
}

// ------------------------------------------------------------------------
// zoo.F1 specific checks
//
//   Operand requirements
//     - rgrp[0].RD  == rgrp[1].RD == rgrp[2].RS2   (note RS2 change)
//     - rgrp[2].RD  == rgrp[3].RD == rgrp[4].RD
//     - rgrp[3].IMM == rgrp[4].IMM  getField IMM not implemented
// ------------------------------------------------------------------------
bool TestBench::f1_constraints(FusionGroupType & g, InstPtrListType & in,
                               InstPtrListType & out)
{
    // This goup expects at least 5 instruction positions in the input
    if (in.size() < 5)
        return false;

    // number of wr/rd ports required by group tested against machine
    // limits
    if (g.fe().getIntWrPorts(in) > g.mi().maxIntWrPorts())
        return false;
    if (g.fe().getIntRdPorts(in) > g.mi().maxIntRdPorts())
        return false;

    // using enum FieldExtractor::FieldName; requires c++20
    constexpr auto RD = FieldExtractor::FieldName::RD;
    constexpr auto RS2 = FieldExtractor::FieldName::RS2;

    // Operand field encodings comparison against constraints
    // The indexes are positions in the group, 0 = 1st instruction
    if (g.fe().noteq(in, 0, 1, RD) || g.fe().noteq(in, 0, 2, RD, RS2)
        || g.fe().noteq(in, 2, 3, RD) || g.fe().noteq(in, 2, 4, RD))
    // || g.fe().noteq(in,3,4,IMM)) FIXME: IMM not implemented yet
    {
        return false;
    }

    // This test only does constraints checking - fake a transform
    out.emplace_back(in[0]);
    in.clear();
    return true;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
fusion::HashType
TestBench::jenkins_1aat(const ::vector<fusion::UidType> & v)
{
    fusion::HashType hash = 0;

    for (auto i : v)
    {
        hash += i;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}

// --------------------------------------------------------------------
void TestBench::info(InstUidListType & aUIDs, InstUidListType & bUIDs,
                     InstPtrListType & instrs)
{
    cout << "aUIDs ";
    for (auto uid : aUIDs)
    {
        cout << " 0x" << setw(8) << hex << setfill('0') << uid;
    }
    cout << endl;
    cout << "bUIDs ";
    for (auto uid : bUIDs)
    {
        cout << " 0x" << setw(8) << hex << setfill('0') << uid;
    }
    cout << endl;

    cout << "Instrs" << endl;
    string pad = "            ";
    for (auto inst : instrs)
    {
        cout << pad << inst << endl;
    }
}
