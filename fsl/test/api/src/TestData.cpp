// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh, Condor Computing Corp.
//
#include "fsl_api/FieldExtractor.h"
#include "fsl_api/Fusion.h"
#include "fsl_api/FusionContext.h"
#include "fsl_api/FusionGroup.h"
#include "fsl_api/FusionTypes.h"
#include "fsl_api/MachineInfo.h"
#include "fsl_api/RadixTrie.h"
#include "Msg.h"
#include "Options.h"
#include "ApiTestBench.h"

#include <boost/range/combine.hpp>

#include <filesystem>
#include <unordered_map>
#include <vector>
using namespace std;
// --------------------------------------------------------------------
// Example
// --------------------------------------------------------------------
//  TestBench::OpcodeListType TestBench::of1 = {
//            // INDEX DISASM             FORM     FIELDS       UID
//    0x76e9, //   0   c.lui  x13, -6    CI-TYPE   RD IMM       0xb
//    0x0685, //   1   c.addi x13,0x1     I-TYPE   RD RS1 IMM   0xd
//    0x8d35, //   2   c.xor  x10,x13     R-TYPE   RD RS1 RS2   0x1c
//    0x1542, //   3   c.slli x10,48      I-TYPE   RD RS1 IMM   0xf
//    0x9141  //   4   c.srli x10,48      I-TYPE   RD RS1 IMM   0x13
//  };
//
//  0  U  RD = G1  IMM = C1
//  1  I  RD = G1  RS1 = G1  IMM = C2
//  2  R  RD = G1  RS1 = G2  RD2 = G1
//  3  I  RD = G2  RS1 = G2  IMM = C3
//  4  I  RD = G2  RS1 = G2  IMM = C3
// ----------------------------------------------------------------------
TestBench::InstUidListType TestBench::uf1 = {0xb, 0xd, 0x1c, 0xf, 0x13};
TestBench::OpcodeListType TestBench::of1 = {
    // INDEX DISASM             FORM     FIELDS       UID
    0x76e9, //   0   c.lui  x13, -6    CI-TYPE   RD IMM       0xb
    0x0685, //   1   c.addi x13,0x1     I-TYPE   RD RS1 IMM   0xd
    0x8d35, //   2   c.xor  x10,x13     R-TYPE   RD RS1 RS2   0x1c
    0x1542, //   3   c.slli x10,48      I-TYPE   RD RS1 IMM   0xf
    0x9141  //   4   c.srli x10,48      I-TYPE   RD RS1 IMM   0x13
};
// fusion::HashType TestBench::hf1   = { 0xb,0xd,0x1c,0xf,0x13 };
//  --------------------------------------------------------------------
//  Fragment of of1
//  --------------------------------------------------------------------
TestBench::InstUidListType TestBench::uf1_1 = {0xd, 0x1c, 0xf, 0x13};
TestBench::OpcodeListType TestBench::of1_1 = {
    0x0685, //  "c.addi x13,0x1",   0xd
    0x8d35, //  "c.xor  x10,x13",   0x1c
    0x1542, //  "c.slli x10,48",    0xf
    0x9141  //  "c.srli x10,48"     0x13
};
//// --------------------------------------------------------------------
TestBench::InstUidListType TestBench::uf1_2 = {0x1c, 0xf, 0x13};
TestBench::OpcodeListType TestBench::of1_2 = {
    0x8d35, //  "c.xor  x10,x13",   0xf
    0x1542, //  "c.slli x10,48",    0xf
    0x9141  //  "c.srli x10,48"     0x13
};
// --------------------------------------------------------------------
TestBench::InstUidListType TestBench::uf1_3 = {0xf, 0x13};
TestBench::OpcodeListType TestBench::of1_3 = {
    0x1542, //  "c.slli x10,48",    0xf
    0x9141  //  "c.srli x10,48"     0x13
};
// --------------------------------------------------------------------
// --------------------------------------------------------------------
TestBench::InstUidListType TestBench::uf2 = {0xd, 0x34};
TestBench::OpcodeListType TestBench::of2 = {
    0x7159, //  "c.addi16sp -112",  0xd
    0xf0a2  //  "c.fswsp f8, 96"    0x34
};
// --------------------------------------------------------------------
// --------------------------------------------------------------------
TestBench::InstUidListType TestBench::uf3 = {0x17, 0x2d};
TestBench::OpcodeListType TestBench::of3 = {
    0x843a, //  "c.mv x8, x14",     0x17
    0x6018  //  "c.flw f14, 0(x8)"  0x2d
};
// --------------------------------------------------------------------
// --------------------------------------------------------------------
TestBench::InstUidListType TestBench::uf4 = {0xf, 0x13};
TestBench::OpcodeListType TestBench::of4 = {
    0xe014, //  "c.fsw f13, 0(x8)",  0xf
    0x86a2  //  "c.mv x13, x8";      0x13
};
// --------------------------------------------------------------------
// --------------------------------------------------------------------
TestBench::InstUidListType TestBench::uf5 = {0x17, 0x2d, 0x34, 0x17, 0x4};
TestBench::OpcodeListType TestBench::of5 = {
    0x843a, //  "c.mv x8, x14",       0x17
    0x6018, //  "c.flw f14, 0(x8)",   0x2d
    0xe014, //  "c.fsw f13, 0(x8)",   0x34
    0x86a2, //  "c.mv x13, x8",       0x17
    0xff65  //  "c.bnez x14, -8"      0x4
};
// --------------------------------------------------------------------
TestBench::InstUidListType TestBench::uf5_1 = {0x17, 0x2d, 0x34, 0x17};
TestBench::OpcodeListType TestBench::of5_1 = {
    0x843a, //  "c.mv x8, x14",      0x17
    0x6018, //  "c.flw f14, 0(x8)",  0x2d
    0xe014, //  "c.fsw f13, 0(x8)",  0x34
    0x86a2  //  "c.mv x13, x8"       0x17
};
// --------------------------------------------------------------------
TestBench::InstUidListType TestBench::uf5_2 = {0x2d, 0x34, 0x17, 0x4};
TestBench::OpcodeListType TestBench::of5_2 = {
    0x6018, //   "c.flw f14, 0(x8)",  0x2d
    0xe014, //   "c.fsw f13, 0(x8)",  0x34
    0x86a2, //   "c.mv x13, x8",      0x17
    0xff65, //   "c.bnez x14, -8"     0x4
};
// --------------------------------------------------------------------
TestBench::InstUidListType TestBench::uf5_3 = {0x2d, 0x34, 0x17};
TestBench::OpcodeListType TestBench::of5_3 = {
    0x6018, //  "c.flw f14, 0(x8)",   0x2d
    0xe014, //  "c.fsw f13, 0(x8)",   0x34
    0x86a2  //  "c.mv x13, x8"        0x17
};
// --------------------------------------------------------------------
// --------------------------------------------------------------------
 const vector<string> TestBench::std_isa_files = {
    "../../../mavis/json/isa_rv64g.json",
    "../../../mavis/json/isa_rv64c.json"};
