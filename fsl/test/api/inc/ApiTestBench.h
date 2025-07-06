// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file TestBench.h  testbench interface and utils
#pragma once
#include "fsl_api/FieldExtractor.h"
#include "fsl_api/Fusion.h"
#include "fsl_api/FusionGroup.h"
#include "fsl_api/FusionTypes.h"
#include "fsl_api/MachineInfo.h"
#include <string>
#include <vector>
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
    struct cbProxy;

    //! \brief run all tests
    bool run();

    //! \brief sanity check compilation of ctors
    bool fusionCtorCompileTest(bool debug = false);

    //! \brief basic find fusiongroup, match to input, and transform
    bool fusionSearchTest(bool debug = false);

    //! \brief sanity check the way mavis and isa files supplied are used
    bool basicMavisTest(bool debug = false);

    //! \brief check constraints and perform simple transform
    bool basicConstraintsTest();

    //! \brief test using alternatives to MachineInfo and FieldExtractor
    bool fusionGroupAltCtorTest();

    //! \brief test choices in specifying FusionGroupCfg
    bool fusionGroupCfgCtorTest();

    //! \brief unit test for class::FusionContext
    bool fusionContextTest(bool debug = false);

    //! \brief unit test for class::RadixTrie
    bool radixTrieTest(bool debug = false);

    //! \brief catch all for start up checks
    bool sanityTest(bool debug = false);

    // -------------------------------------------------------------
    // field extractor method tests and support, testfieldextractor.cpp
    // -------------------------------------------------------------
    //! \brief top level extractor test runner
    bool fieldExtractorTests(bool debug = false);

    //! \brief create an inst from opcode, catch conversion errors
    FieldExtractor::InstPtrType makeInst(MavisType &, uint32_t);

    //! \brief helper for testing field values
    bool testFieldValue(uint32_t, std::string, uint32_t act,uint32_t exp);

    // -------------------------------------------------------------
    // fusion domain language tests and support
    // -------------------------------------------------------------
    //! \brief this calls the fsl sub-tests
    bool fslTests(bool debug = false);

    //! \brief simple sanity check on interpreter linking
    //!
    //! See fsl/test/interp for more extensive syntax checking
    bool fslInterpQuickTest(bool debug = false);

    //! \brief support for fsl_syntax_test
    bool checkSyntax(std::vector<std::string> &, bool debug = false);

    //! \brief return true if files are identical
    //!
    //! whitespace is significant
    bool compareFiles(const std::string,const std::string,bool=true);
    // -------------------------------------------------------------

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
    fusion::HashType jenkinsOneAtATime(const std::vector<fusion::UidType> &);

    //! \brief common isa files to separate from the cmd line versions
    static const std::vector<std::string> stdIsaFiles;

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

    static const std::vector<std::string> std_isa_files;
};

// ---------------------------------------------------------------------
//! \brief call back proxies used in the unit test(s)
//!
//! There is a callback for each fusion group test case  f1, f1.1, etc
// ---------------------------------------------------------------------
struct TestBench::cbProxy
{
    //! \brief ...
    static bool uf1_func(FusionGroupType &, InstPtrListType &,
                         InstPtrListType &)
    {
        cout << "HERE uf1_func called" << endl;
        return true;
    }

    //! \brief ...
    static bool uf1_1_func(FusionGroupType &, InstPtrListType &,
                           InstPtrListType &)
    {
        cout << "HERE uf1_1_func called" << endl;
        return true;
    }

    //! \brief ...
    static bool uf1_2_func(FusionGroupType &, InstPtrListType &,
                           InstPtrListType &)
    {
        cout << "HERE uf1_2_func called" << endl;
        return true;
    }

    //! \brief ...
    static bool uf1_3_func(FusionGroupType &, InstPtrListType &,
                           InstPtrListType &)
    {
        cout << "HERE uf1_3_func called" << endl;
        return true;
    }

    //! \brief ...
    static bool uf2_func(FusionGroupType &, InstPtrListType &,
                         InstPtrListType &)
    {
        cout << "HERE uf2_func called" << endl;
        return true;
    }

    //! \brief ...
    static bool uf3_func(FusionGroupType &, InstPtrListType &,
                         InstPtrListType &)
    {
        cout << "HERE uf3_func called" << endl;
        return true;
    }

    //! \brief ...
    static bool uf4_func(FusionGroupType &, InstPtrListType &,
                         InstPtrListType &)
    {
        cout << "HERE uf4_func called" << endl;
        return true;
    }

    //! \brief ...
    static bool uf5_func(FusionGroupType &, InstPtrListType &,
                         InstPtrListType &)
    {
        cout << "HERE uf5_func called" << endl;
        return true;
    }

    //! \brief ...
    static bool uf5_1_func(FusionGroupType &, InstPtrListType &,
                           InstPtrListType &)
    {
        cout << "HERE uf5_1_func called" << endl;
        return true;
    }

    //! \brief ...
    static bool uf5_2_func(FusionGroupType &, InstPtrListType &,
                           InstPtrListType &)
    {
        cout << "HERE uf5_2_func called" << endl;
        return true;
    }

    //! \brief ...
    static bool uf5_3_func(FusionGroupType &, InstPtrListType &,
                           InstPtrListType &)
    {
        cout << "HERE uf5_3_func called" << endl;
        return true;
    }
};
