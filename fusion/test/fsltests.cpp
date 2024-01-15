#include "fusion.h"
#include "qparser.h"
#include "msg.h"
#include "options.h"
#include "testbench.h"
#include "qparser.h"

#include <filesystem>
using namespace std;
extern QParser* QP;
extern uint32_t lineNo;

// --------------------------------------------------------------------
// More tests will be written
// --------------------------------------------------------------------
bool TestBench::fsl_tests(bool)
{
    if (!fsl_syntax_test())
        return false;

    return true;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
bool TestBench::check_syntax(vector<string> & files, bool)
{
    bool ok = true;
    for (auto fn : files)
    {

        if (verbose)
        {
            std::filesystem::path fp(fn);
            string fandext =
                fp.filename().string() + fp.extension().string();
            msg->imsg("parsing " + fandext);
        }

        QP->warmReset();

        if (!(QP->parse(fn)))
        {
            // yyerror() reports the error message
            ok = false;
        }
    }

    return ok;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
bool TestBench::fsl_syntax_test(bool)
{
    if (verbose)
        msg->imsg("fsl_syntax_test BEGIN");

    // There should be at least one file for this test
    if (opts->fsl_syntax_files.size() == 0)
    {
        msg->emsg("No FSL syntax test files specified");
        return false;
    }

    bool ok = true;

    QP->coldReset();

    // Check the files specifically that hold syntax corner cases
    if (!check_syntax(opts->fsl_syntax_files))
        ok = false;

    stringstream ss;
    QP->symtab.info(ss, true);

    if (ss.str() != symbolTableExpectData)
    {
        ok = false;

        msg->emsg("Symbol table does not match expect");
        msg->emsg("  Actual data symtab_actual.txt");
        msg->emsg("  Expect data symtab_expect.txt");

        ofstream actual("symtab_actual.txt");
        actual << ss.str() << endl;
        actual.close();

        ofstream expect("symtab_expect.txt");
        expect << symbolTableExpectData << endl;
        expect.close();
    }

    // Check all the other files, looking for thing to add to corner cases
    if (!check_syntax(opts->fsl_files))
        ok = false;

    if (verbose)
        msg->imsg("fsl_syntax_test END");

    return ok;
}

const std::string TestBench::symbolTableExpectData =
    "\n"
    "---------------------------------------------------------------------"
    "----\n"
    "Symbol table\n"
    "Name   Type                  Line     File                           "
    "     \n"
    "---------------------------------------------------------------------"
    "----\n"
    "word1  ENCODING_DECL_NAME    24      "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "_opt0  OPT_TYPE              14      "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "c3     UNSIGNED              13      "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "opc    UNSIGNED              24      "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "cons1  CONSTRAINTS_DECL_NAME 18      "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "g2     GPR                   12      "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "fs1    FUSION_DECL_NAME      5       "
    "....l-f2/fusion/test/fsl/syntax1.fsl\n"
    "fs2    FUSION_DECL_NAME      9       "
    "....l-f2/fusion/test/fsl/syntax1.fsl\n"
    "oly1   UARCH_DECL_NAME       5       "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "in_seq INPUT_SEQ_DECL_NAME   6       "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "c.lui  MNEMONIC              9       "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "c.srli MNEMONIC              15      "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "seq1   SEQUENCE_DECL_NAME    8       "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "_req0  REQ_TYPE              11      "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "0x234  UNKNOWN               33      "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "rv64g  ISA_DECL_NAME         4       "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "g1     GPR                   9       "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "t1     TRANSFORM_DECL_NAME   23      "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "c1     SIGNED                9       "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "c.addi MNEMONIC              10      "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "c.slli MNEMONIC              13      "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "c2     SIGNED                10      "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "c.xor  MNEMONIC              12      "
    "....l-f2/fusion/test/fsl/syntax2.fsl\n"
    "\n"
    "Symbol table total entries 23\n"
    "\n";
