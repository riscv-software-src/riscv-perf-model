// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh, Condor Computing Corp.
//
#include "Msg.h"
#include "Options.h"
#include "ApiTestBench.h"
#include <sstream>
#include <iostream>
using namespace std;

// --------------------------------------------------------------------
// The intent is at least one test for the top level methods overall.
//
// Some method tests are covered as part of the Fusion Group and transform
// tests. SField and ImmField tests are less used elsewhere.
// These outliers are tested here.
// --------------------------------------------------------------------
bool TestBench::fieldExtractorTests(bool debug)
{
    FieldExtractor fe;
    using InstPtrType = FieldExtractor::InstPtrType;

    if (verbose)
        msg->imsg("fieldExtractorTests BEGIN");

    bool ok = true;
    MavisType mavis(opts->isa_files, {});

    // add x1,x2,x3
    InstPtrType inst_1 = makeInst(mavis, 0x003100b3);

    if (inst_1 == nullptr)
        ok = false;

    using FN = FieldExtractor::FieldName;
    using SFN = FieldExtractor::SFieldName;

    // Test for uint32_t getField(InstPtrType,FieldName) const
    uint32_t rd_inst_1 = fe.getField(inst_1, FN::RD);
    uint32_t rs1_inst_1 = fe.getField(inst_1, FN::RS1);
    uint32_t rs2_inst_1 = fe.getField(inst_1, FN::RS2);

    if (!testFieldValue(0, "RD", rd_inst_1, 0x1))
        ok = false;
    if (!testFieldValue(1, "RS1", rs1_inst_1, 0x2))
        ok = false;
    if (!testFieldValue(2, "RS2", rs2_inst_1, 0x3))
        ok = false;

    bool isDest = false;
    if (!fe.checkInstHasField(inst_1, FN::RD, isDest))
    {
        msg->emsg("checkInstHasField() failed to detect RD");
        ok = false;
    }

    if (!isDest)
    {
        msg->emsg("ID=3: checkInstHasField() failed to set isDest");
        ok = false;
    }

    // Test for uint32_t getSField(InstPtrType,SFieldName) const
    //  [ funct7 | rs2 | rs1 | rm | rd | opcode ]
    //  3322 2222 2222 1111 1111 11
    //  1098 7654 3210 9876 5432 1098 7654 3210
    //  0111 0010 1010 0111 1111 0101 0100 0011
    //  RM should be  111 -> 0x7
    InstPtrType inst_2 = makeInst(mavis, 0x72a7f543);
    if (inst_2 == nullptr)
        ok = false;
    uint32_t rm_inst_2 = fe.getSField(inst_2, SFN::RM);
    if (!testFieldValue(4, "RM", rm_inst_2, 0x7))
        ok = false;

    // Test for uint32_t getImmField(InstPtrType inst) const

    if (!ok)
        msg->emsg("fieldExtractor_tests FAILED");
    if (verbose)
        msg->imsg("fieldExtractor_tests END");
    return ok;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
FieldExtractor::InstPtrType TestBench::makeInst(MavisType & m,
                                                 uint32_t opc)
{
    FieldExtractor::InstPtrType inst;

    try
    {
        inst = m.makeInst(opc, 0);
    }
    catch (const std::exception &)
    {
        if (!inst)
        {
            std::ostringstream ss;
            ss << "0x" << std::setw(8) << std::setfill('0') << std::hex
               << opc;
            msg->emsg("Mavis could not create instruction from "
                      + ss.str());
            return nullptr;
        }
    }

    return inst;
}

// --------------------------------------------------------------------
// --------------------------------------------------------------------
bool TestBench::testFieldValue(uint32_t id, string name, uint32_t act,
                                 uint32_t exp)
{
    if (act != exp)
    {
        msg->emsg("ID:" + ::to_string(id) + ":FIELD:" + name
                  + ": value mismatch");
        return false;
    }

    return true;
}
