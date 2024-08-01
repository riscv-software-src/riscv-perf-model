#!/usr/bin/env python

import os
import json

MAVIS = os.environ.get('MAVIS', "../../mavis/")
JSON  = os.environ.get('JSON', "olympia_uarch_rv64v.json")

SUPPORTED_INSTS = {
# Vector Config Setting Instructions
    "vsetvli" :    {"pipe" : "vset", "latency" : 1},
    "vsetvl" :     {"pipe" : "vset", "latency" : 1},
    "vsetivli" :   {"pipe" : "vset", "latency" : 1},

# TODO: Vector Loads and Stores: Vector Unit-Stride Instructions
    "vse8.v"  :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
    "vse16.v" :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
    "vse32.v" :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
    "vse64.v" :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
    "vle8.v"  :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
    "vle16.v" :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
    "vle32.v" :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
    "vle64.v" :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
# TODO: Vector Loads and Stores: Vector Strided Instructions
    "vsse8.v"  :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
    "vsse16.v" :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
    "vsse32.v" :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
    "vsse64.v" :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
    "vlse8.v"  :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
    "vlse16.v" :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
    "vlse32.v" :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
    "vlse64.v" :      {"pipe" : "vlsu", "uop_gen" : "ARITH", "latency" : 1},
# TODO: Vector Loads and Stores: Vector Indexed Instructions
# TODO: Vector Loads and Stores: Unit-stride Fault-Only-First Loads
# TODO: Vector Loads and Stores: Vector Load/Store Segment Instructions
# TODO: Vector Loads and Stores: Vector Load/Store Whole Register Instructions

# Vector Integer Arithmetic Instructions: Vector Single-Width Integer Add and Subtract
    "vadd.vv" :    {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vadd.vx" :    {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vadd.vi" :    {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsub.vv" :    {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsub.vx" :    {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vrsub.vi" :   {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vrsub.vx" :   {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},

# Vector Integer Arithmetic Instructions: Vector Widening Integer Add/Subtract
    "vwaddu.vv" :  {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwaddu.vx" :  {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwsubu.vv" :  {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwsubu.vx" :  {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwadd.vv" :   {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwadd.vx" :   {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwsub.vv" :   {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwsub.vx" :   {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwaddu.wv" :  {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwaddu.wx" :  {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwsubu.wv" :  {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwsubu.wx" :  {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwadd.wv" :   {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwadd.wx" :   {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwsub.wv" :   {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},
    "vwsub.wx" :   {"pipe" : "vint", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 1},

# TODO: Vector Integer Arithmetic Instructions: Vector Integer Extension
# TODO: Vector Integer Arithmetic Instructions: Vector Integer Add-with-Carry/Subtract-with-Borrow Instructions
# Vector Integer Arithmetic Instructions: Vector Bitwise Logical Instructions
    "vand.vv" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vand.vx" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vand.vi" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vor.vv" :  {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vor.vx" :  {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vor.vi" :  {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vxor.vv" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vxor.vx" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vxor.vi" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},

# TODO: Vector Integer Arithmetic Instructions: Vector Single-Width Shift Instructions
# TODO: Vector Integer Arithmetic Instructions: Vector Narrowing Integer Right Shift Instructions
# Vector Integer Arithmetic Instructions: Vector Integer Compare Instructions
    "vmseq.vv" :  {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmseq.vx" :  {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmseq.vi" :  {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsne.vv" :  {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsne.vx" :  {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsne.vi" :  {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsltu.vv" : {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsltu.vx" : {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmslt.vv" :  {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmslt.vx" :  {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsleu.vv" : {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsleu.vx" : {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsleu.vi" : {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsle.vv" :  {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsle.vx" :  {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsle.vi" :  {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsgtu.vx" : {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsgtu.vi" : {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsgt.vx" :  {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},
    "vmsgt.vi" :  {"pipe" : "vint", "uop_gen" : "ARITH_SINGLE_DEST", "latency" : 1},

# Vector Integer Arithmetic Instructions: Vector Integer Min/Max Instructions
    "vminu.vv" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vminu.vx" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmin.vv" :  {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmin.vx" :  {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmaxu.vv" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmaxu.vx" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmax.vv" :  {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmax.vx" :  {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},

# Vector Integer Arithmetic Instructions: Vector Single-Width Integer Multiply Instructions
    "vmul.vv" :    {"pipe" : "vmul", "uop_gen" : "ARITH", "latency" : 3},
    "vmul.vx" :    {"pipe" : "vmul", "uop_gen" : "ARITH", "latency" : 3},
    "vmulhu.vx" :  {"pipe" : "vmul", "uop_gen" : "ARITH", "latency" : 3},
    "vmulhu.vv" :  {"pipe" : "vmul", "uop_gen" : "ARITH", "latency" : 3},
    "vmulh.vv" :   {"pipe" : "vmul", "uop_gen" : "ARITH", "latency" : 3},
    "vmulh.vx" :   {"pipe" : "vmul", "uop_gen" : "ARITH", "latency" : 3},
    "vmulhsu.vv" : {"pipe" : "vmul", "uop_gen" : "ARITH", "latency" : 3},
    "vmulhsu.vx" : {"pipe" : "vmul", "uop_gen" : "ARITH", "latency" : 3},

# Vector Integer Arithmetic Instructions: Vector Integer Divide Instructions
    "vdiv.vv" :    {"pipe" : "vdiv", "uop_gen" : "ARITH", "latency" : 23},
    "vdiv.vx" :    {"pipe" : "vdiv", "uop_gen" : "ARITH", "latency" : 23},
    "vdivu.vv" :   {"pipe" : "vdiv", "uop_gen" : "ARITH", "latency" : 23},
    "vdivu.vx" :   {"pipe" : "vdiv", "uop_gen" : "ARITH", "latency" : 23},
    "vremu.vv" :   {"pipe" : "vdiv", "uop_gen" : "ARITH", "latency" : 23},
    "vremu.vx" :   {"pipe" : "vdiv", "uop_gen" : "ARITH", "latency" : 23},
    "vrem.vv" :    {"pipe" : "vdiv", "uop_gen" : "ARITH", "latency" : 23},
    "vrem.vx" :    {"pipe" : "vdiv", "uop_gen" : "ARITH", "latency" : 23},

# Vector Integer Arithmetic Instructions: Vector Widening Integer Multiply Instructions
    "vwmul.vv" :   {"pipe" : "vmul", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 3},
    "vwmul.vx" :   {"pipe" : "vmul", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 3},
    "vwmulu.vv" :  {"pipe" : "vmul", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 3},
    "vwmulu.vx" :  {"pipe" : "vmul", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 3},
    "vwmulsu.vv" : {"pipe" : "vmul", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 3},
    "vwmulsu.vx" : {"pipe" : "vmul", "uop_gen" : "ARITH_WIDE_DEST", "latency" : 3},

# TODO: Vector Integer Arithmetic Instructions: Vector Single-Width Integer Multiply-Add Instructions
# TODO: Vector Integer Arithmetic Instructions: Vector Widening Integer Multiply-Add Instructions
# TODO: Vector Integer Arithmetic Instructions: Vector Integer Merge Instructions
# TODO: Vector Integer Arithmetic Instructions: Vector Integer Move Instructions
# TODO: Vector Fixed-Point Arithmetic Instructions: Vector Single-Width Saturating Add and Subtract
# TODO: Vector Fixed-Point Arithmetic Instructions: Vector Single-Width Averaging Add and Subtract
# Vector Fixed-Point Arithmetic Instructions: Vector Single-Width Fractional Multiply with Rounding and Saturation
    "vsmul.vx" :   {"pipe" : "vmul", "uop_gen" : "ARITH", "latency" : 3},
    "vsmul.vv" :   {"pipe" : "vmul", "uop_gen" : "ARITH", "latency" : 3},

# TODO: Vector Fixed-Point Arithmetic Instructions: Vector Single-Width Scaling Shift Instructions
# TODO: Vector Fixed-Point Arithmetic Instructions: Vector Narrowing Fixed-Point Clip Instructions
# TODO: Vector Floating-Point Instructions: Vector Floating-Point Exception Flags
# TODO: Vector Floating-Point Instructions: Vector Single-Width Floating-Point Add/Subtract Instructions
# TODO: Vector Floating-Point Instructions: Vector Widening Floating-Point Add/Subtract Instructions
# TODO: Vector Floating-Point Instructions: Vector Single-Width Floating-Point Multiply/Divide Instructions
# TODO: Vector Floating-Point Instructions: Vector Widening Floating-Point Multiply
# TODO: Vector Floating-Point Instructions: Vector Single-Width Floating-Point Fused Multiply-Add Instructions
# TODO: Vector Floating-Point Instructions: Vector Widening Floating-Point Fused Multiply-Add Instructions
# TODO: Vector Floating-Point Instructions: Vector Floating-Point Square-Root Instruction
# TODO: Vector Floating-Point Instructions: Vector Floating-Point Reciprocal Square-Root Estimate Instruction
# TODO: Vector Floating-Point Instructions: Vector Floating-Point Reciprocal Estimate Instruction
# TODO: Vector Floating-Point Instructions: Vector Floating-Point MIN/MAX Instructions
# TODO: Vector Floating-Point Instructions: Vector Floating-Point Sign-Injection Instructions
# TODO: Vector Floating-Point Instructions: Vector Floating-Point Compare Instructions
# TODO: Vector Floating-Point Instructions: Vector Floating-Point Classify Instruction
# TODO: Vector Floating-Point Instructions: Vector Floating-Point Merge Instruction
# TODO: Vector Floating-Point Instructions: Vector Floating-Point Move Instruction
# TODO: Vector Floating-Point Instructions: Single-Width Floating-Point/Integer Type-Convert Instructions
# TODO: Vector Floating-Point Instructions: Widening Floating-Point/Integer Type-Convert Instructions
# TODO: Vector Floating-Point Instructions: Narrowing Floating-Point/Integer Type-Convert Instructions
# TODO: Vector Reduction Operations: Vector Single-Width Integer Reduction Instructions
# TODO: Vector Reduction Operations: Vector Widening Integer Reduction Instructions
# TODO: Vector Reduction Operations: Vector Single-Width Floating-Point Reduction Instructions
# TODO: Vector Reduction Operations: Vector Widening Floating-Point Reduction Instructions
# Vector Mask Instructions: Vector Mask-Register Logical Instructions
    "vmandn.mm" : {"pipe" : "vmask", "uop_gen" : "NONE", "latency" : 1},
    "vmand.mm"  : {"pipe" : "vmask", "uop_gen" : "NONE", "latency" : 1},
    "vmor.mm"   : {"pipe" : "vmask", "uop_gen" : "NONE", "latency" : 1},
    "vmxor.mm"  : {"pipe" : "vmask", "uop_gen" : "NONE", "latency" : 1},
    "vmorn.mm"  : {"pipe" : "vmask", "uop_gen" : "NONE", "latency" : 1},
    "vmnand.mm" : {"pipe" : "vmask", "uop_gen" : "NONE", "latency" : 1},
    "vmnor.mm"  : {"pipe" : "vmask", "uop_gen" : "NONE", "latency" : 1},
    "vmxnor.mm" : {"pipe" : "vmask", "uop_gen" : "NONE", "latency" : 1},

# TODO: Vector Mask Instructions: Vector count population in mask vcpop.m
# TODO: Vector Mask Instructions: vfirst nd-rst-set mask bit
# TODO: Vector Mask Instructions: vmsbf.m set-before-rst mask bit
# TODO: Vector Mask Instructions: vmsif.m set-including-rst mask bit
# TODO: Vector Mask Instructions: vmsof.m set-only-rst mask bit
# TODO: Vector Mask Instructions: Vector Iota Instruction
# TODO: Vector Mask Instructions: Vector Element Index Instruction
# TODO: Vector Permutation Instructions: Integer Scalar Move Instructions
# TODO: Vector Permutation Instructions: Floating-Point Scalar Move Instructions
# TODO: Vector Permutation Instructions: Vector Slide Instructions
# TODO: Vector Permutation Instructions: Vector Register Gather Instructions
# TODO: Vector Permutation Instructions: Vector Compress Instruction
# TODO: Vector Permutation Instructions: Whole Vector Register Move
}

# Get a list of all vector insts from Mavis
mavis_rv64v_json = []
with open(MAVIS+"json/isa_rv64v.json", "r") as f:
    mavis_rv64v_json.extend(json.load(f))
with open(MAVIS+"json/isa_rv64vf.json", "r") as f:
    mavis_rv64v_json.extend(json.load(f))

# Sort alphabetically by mnemonic
mavis_rv64v_json.sort(key=lambda opcode: opcode["mnemonic"])

# Construct the uarch json
uarch_json = []
for opcode in mavis_rv64v_json:
    mnemonic = opcode["mnemonic"]

    # Assume inst is unsupported
    opcode_entry = {
        "mnemonic" : mnemonic,
        "pipe" : "?",
        "uop_gen" : "NONE",
        "latency" : 0
    }

    # If it is supported, get the pipe and latency
    if mnemonic in SUPPORTED_INSTS.keys():
        opcode_entry["mnemonic"] = mnemonic
        opcode_entry["pipe"] = SUPPORTED_INSTS[mnemonic]["pipe"]
        if(SUPPORTED_INSTS[mnemonic].get("uop_gen")):
            opcode_entry["uop_gen"] = SUPPORTED_INSTS[mnemonic]["uop_gen"]
        opcode_entry["pipe"] = SUPPORTED_INSTS[mnemonic]["pipe"]
        opcode_entry["latency"] = SUPPORTED_INSTS[mnemonic]["latency"]

    uarch_json.append(opcode_entry)

# Write the json to the file
with open(JSON, "w") as f:
    print("Writing rv64v uarch json to "+JSON)
    json.dump(uarch_json, f, indent=4)

