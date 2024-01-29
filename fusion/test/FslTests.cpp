//// HEADER PLACEHOLDER
//// contact Jeff Nye, jeffnye-gh, Condor Computing Corp.
////
//#include "Fusion.hpp"
//#include "FslParser.hpp"
//#include "Msg.hpp"
//#include "Options.hpp"
//#include "TestBench.hpp"
//
//#include <filesystem>
//using namespace std;
//extern FslParser* FP;
//extern uint32_t lineNo;
//
//// --------------------------------------------------------------------
//// More tests will be written
//// --------------------------------------------------------------------
//bool TestBench::fslTests(bool)
//{
//    if (!fslSyntaxTest())
//        return false;
//
//    return true;
//}

//// --------------------------------------------------------------------
//// --------------------------------------------------------------------
//bool TestBench::checkSyntax(vector<string> & files, bool)
//{
//    bool ok = true;
//    for (auto fn : files)
//    {
//
//        if (verbose)
//        {
//            std::filesystem::path fp(fn);
//            string fandext =
//                fp.filename().string() + fp.extension().string();
//            msg->imsg("parsing " + fandext);
//        }
//
//        FP->warmReset();
//
//        if (!(FP->parse(fn)))
//        {
//            // yyerror() reports the error message
//            ok = false;
//        }
//    }
//
//    return ok;
//}
//
//// --------------------------------------------------------------------
//// --------------------------------------------------------------------
//bool TestBench::fslSyntaxTest(bool)
//{
//    if (verbose)
//        msg->imsg("fslSyntaxTest BEGIN");
//
//    // There should be at least one file for this test
//    if (opts->fsl_syntax_files.size() == 0)
//    {
//        msg->emsg("No FSL syntax test files specified");
//        return false;
//    }
//
//    bool ok = true;
//
//    FP->coldReset();
//
//    // Check the files specifically that hold syntax corner cases
//    if (!checkSyntax(opts->fsl_syntax_files))
//        ok = false;
//
//    //See the cmake command that copies expected to the bin dir.
//    string actualFN = "symtab_actual.txt";
//    string expectFN = "symtab_expect.txt";
//
//    //Write the symtab to a file, (true) use the file name.ext 
//    //only option
//    ofstream actual(actualFN.c_str());
//    FP->symtab.info(actual, true);
//    actual.close();
//
//    // (true) emit differences
//    if(!compareFiles(actualFN,expectFN)) {
//        ok = false;
//        msg->emsg("Symbol table does not match expect");
//        msg->emsg("  Actual data : "+actualFN);
//        msg->emsg("  Expect data : "+expectFN);
//    }
//    
//    // Check all the other files, looking for things to add to 
//    // corner cases tests
//    if (!checkSyntax(opts->fsl_files))
//        ok = false;
//
//    if (verbose)
//        msg->imsg("fslSyntaxTest END");
//
//    return ok;
//}
