// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file testbenchheader.h  testbench interface and utils
#pragma once
#include "fusiontypes.h"
#include "fusion.h"
#include "fusiongroup.h"
#include "machineinfo.h"
#include "fieldextractor.h"
//! \class TestBench
// ====================================================================
// ====================================================================
//! \brief local test bench for Fusion and related classes
struct TestBench
{
    //! \brief save typing
    using FusionGroupType =
        fusion::FusionGroup<MachineInfo, FieldExtractor>;
    //! \brief save typing
    using FusionGroupListType = std::vector<FusionGroupType>;

    //! \brief save typing
    using FusionGroupCfgType =
        fusion::FusionGroupCfg<MachineInfo, FieldExtractor>;

    //! \brief save typing
    using FusionType =
        fusion::Fusion<FusionGroupType, MachineInfo, FieldExtractor>;

    //! \brief save typing
    using InstUidListType = fusion::InstUidListType;
    //! \brief save typing
    using InstPtrListType = fusion::InstPtrListType;
    //! \brief save typing
    using FileNameListType = fusion::FileNameListType;
    //! \brief save typing
    using Opcode = fusion::Opcode;
    //! \brief save typing
    using OpcodeListType = fusion::OpcodeListType;
    //! \brief save typing
    using MavisType = fusion::MavisType;

    //! \brief ctor with boost program_option support
    TestBench(int, char**);
    //! \brief hold functor proxies used by the tb
    struct cb_proxy;

    //! \brief run all tests
    bool run();

    //! \brief sanity check compilation of ctors
    bool fusion_ctor_compile_test(bool debug = false);

    //! \brief basic find fusiongroup, match to input, and transform
    bool fusion_search_test(bool debug = false);

    //! \brief sanity check the way mavis and isa files supplied are used
    bool basic_mavis_test(bool debug = false);

    //! \brief check constraints and perform simple transform
    bool basic_constraints_test();

    //! \brief test using alternatives to MachineInfo and FieldExtractor
    bool fusiongroup_alt_ctor_test();

    //! \brief test choices in specifying FusionGroupCfg
    bool fusiongroup_cfg_ctor_test();

    //! \brief unit test for class::FusionContext
    bool fusion_context_test(bool debug = false);

    //! \brief unit test for class::RadixTrie
    bool radix_trie_test(bool debug = false);

    //! \brief transform funcs
    static bool f1_constraints(FusionGroupType &, InstPtrListType &,
                               InstPtrListType &);

    //! \brief assign to InstPtrListType from opcodes
    void assign(InstPtrListType &, std::vector<Opcode> &,
                const FileNameListType &);
    //! \brief info debug function
    void info(InstUidListType &, InstUidListType &, InstPtrListType &);
    //! \brief support golden reference hashes
    void generateExpectHashes(
        std::unordered_map<std::string, fusion::HashType> &,
        const FusionType::FusionGroupCfgListType &);
    //! \brief duplicate of hash function found in fusion group
    //! 
    //! duplicated  to support debug
    fusion::HashType jenkins_1aat(const std::vector<fusion::UidType> &);

    //! \brief common isa files to separate from the cmd line versions
    static const std::vector<std::string> std_isa_files;
    //! \brief extra messages
    bool verbose{false};

    //! \brief zoo.F1  example fusion group
    //!
    //! This is from the fusion group zoo, F1 has three variants. There are
    //! three forms for each, opcode, uid, and asm text. I'm trying out
    //! various methods for a user to express a fusion group when not using
    //! the DSL. Note all of these will be needed.
    //! @{
    static InstUidListType uf1, uf1_1, uf1_2, uf1_3;
    static OpcodeListType of1, of1_1, of1_2, of1_3;
    //! @}

    //! \brief zoo.F2 example fusion group
    //! @{
    static InstUidListType uf2;
    static OpcodeListType of2;
    //! @}

    //! \brief zoo.F3 example fusion group
    //! @{
    static InstUidListType uf3;
    static OpcodeListType of3;
    //! }@

    //! \brief zoo.F4 example fusion group
    //! @{
    static InstUidListType uf4;
    static OpcodeListType of4;
    //! }@

    //! \brief zoo.F5 example fusion group and three variants
    //! @{
    static InstUidListType uf5, uf5_1, uf5_2, uf5_3;
    static OpcodeListType of5, of5_1, of5_2, of5_3;
    //! }@
};
