#!/usr/bin/env python

import os
import json

MAVIS = os.environ.get('MAVIS', "../../mavis/")
JSON  = os.environ.get('JSON', "olympia_uarch_rv64v.json")

SUPPORTED_INSTS = {
    "vadd.vv" :    {"pipe" : "vint", "latency" : 1},
    "vadd.vx" :    {"pipe" : "vint", "latency" : 1},
    "vadd.vi" :    {"pipe" : "vint", "latency" : 1},
    "vsub.vv" :    {"pipe" : "vint", "latency" : 1},
    "vsub.vx" :    {"pipe" : "vint", "latency" : 1},
    "vrsub.vi" :   {"pipe" : "vint", "latency" : 1},
    "vrsub.vx" :   {"pipe" : "vint", "latency" : 1},
    "vsetvli" :    {"pipe" : "vset", "latency" : 1},
    "vsetvl" :     {"pipe" : "vset", "latency" : 1},
    "vsetivli" :   {"pipe" : "vset", "latency" : 1},
    "vmul.vv" :    {"pipe" : "vmul", "latency" : 3},
    "vmul.vx" :    {"pipe" : "vmul", "latency" : 3},
    "vmulhu.vx" :  {"pipe" : "vmul", "latency" : 3},
    "vmulhu.vv" :  {"pipe" : "vmul", "latency" : 3},
    "vmulh.vv" :   {"pipe" : "vmul", "latency" : 3},
    "vmulh.vx" :   {"pipe" : "vmul", "latency" : 3},
    "vmulhsu.vv" : {"pipe" : "vmul", "latency" : 3},
    "vmulhsu.vx" : {"pipe" : "vmul", "latency" : 3},
    "vdiv.vv" :    {"pipe" : "vdiv", "latency" : 23},
    "vdiv.vx" :    {"pipe" : "vdiv", "latency" : 23},
    "vdivu.vv" :   {"pipe" : "vdiv", "latency" : 23},
    "vdivu.vx" :   {"pipe" : "vdiv", "latency" : 23},
    "vremu.vv" :   {"pipe" : "vdiv", "latency" : 23},
    "vremu.vx" :   {"pipe" : "vdiv", "latency" : 23},
    "vrem.vv" :    {"pipe" : "vdiv", "latency" : 23},
    "vrem.vx" :    {"pipe" : "vdiv", "latency" : 23},
    "vwmul.vv" :   {"pipe" : "vmul", "latency" : 3},
    "vwmul.vx" :   {"pipe" : "vmul", "latency" : 3},
    "vwmulu.vv" :  {"pipe" : "vmul", "latency" : 3},
    "vwmulu.vx" :  {"pipe" : "vmul", "latency" : 3},
    "vwmulsu.vv" : {"pipe" : "vmul", "latency" : 3},
    "vwmulsu.vx" : {"pipe" : "vmul", "latency" : 3},
    "vwaddu.vv" :  {"pipe" : "vint", "latency" : 1},
    "vwaddu.vx" :  {"pipe" : "vint", "latency" : 1},
    "vwsubu.vv" :  {"pipe" : "vint", "latency" : 1},
    "vwsubu.vx" :  {"pipe" : "vint", "latency" : 1},
    "vwadd.vv" :   {"pipe" : "vint", "latency" : 1},
    "vwadd.vx" :   {"pipe" : "vint", "latency" : 1},
    "vwsub.vv" :   {"pipe" : "vint", "latency" : 1},
    "vwsub.vx" :   {"pipe" : "vint", "latency" : 1},
    "vwaddu.wv" :  {"pipe" : "vint", "latency" : 1},
    "vwaddu.wx" :  {"pipe" : "vint", "latency" : 1},
    "vwsubu.wv" :  {"pipe" : "vint", "latency" : 1},
    "vwsubu.wx" :  {"pipe" : "vint", "latency" : 1},
    "vwadd.wv" :   {"pipe" : "vint", "latency" : 1},
    "vwadd.wx" :   {"pipe" : "vint", "latency" : 1},
    "vwsub.wv" :   {"pipe" : "vint", "latency" : 1},
    "vwsub.wx" :   {"pipe" : "vint", "latency" : 1},
    "vsmul.vx" :   {"pipe" : "vmul", "latency" : 3},
    "vsmul.vv" :   {"pipe" : "vmul", "latency" : 3}
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
        "latency" : 0
    }

    # If it is supported, get the pipe and latency
    if mnemonic in SUPPORTED_INSTS.keys():
        opcode_entry["mnemonic"] = mnemonic
        opcode_entry["pipe"] = SUPPORTED_INSTS[mnemonic]["pipe"]
        opcode_entry["latency"] = SUPPORTED_INSTS[mnemonic]["latency"]

    uarch_json.append(opcode_entry)

# Write the json to the file
with open(JSON, "w") as f:
    print("Writing rv64v uarch json to "+JSON)
    json.dump(uarch_json, f, indent=4)

