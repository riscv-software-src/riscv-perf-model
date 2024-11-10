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
# TODO: Vector Loads and Stores: Vector Strided Instructions
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
# FIXME: Requires Mavis fix to support correctly
#    "vzext.vf2" : {"pipe" : "vint", "uop_gen" : "ARITH_EXT", "latency" : 1},
#    "vsext.vf2" : {"pipe" : "vint", "uop_gen" : "ARITH_EXT", "latency" : 1},
#    "vzext.vf4" : {"pipe" : "vint", "uop_gen" : "ARITH_EXT", "latency" : 1},
#    "vsext.vf4" : {"pipe" : "vint", "uop_gen" : "ARITH_EXT", "latency" : 1},
#    "vzext.vf8" : {"pipe" : "vint", "uop_gen" : "ARITH_EXT", "latency" : 1},
#    "vsext.vf8" : {"pipe" : "vint", "uop_gen" : "ARITH_EXT", "latency" : 1},

# Vector Integer Arithmetic Instructions: Vector Integer Add-with-Carry/Subtract-with-Borrow Instructions
# FIXME: Requires Mavis fix to include vector mask
    "vadc.vvm"  : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vadc.vxm"  : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vadc.vim"  : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmadc.vvm" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmadc.vxm" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmadc.vim" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmadc.vv"  : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmadc.vx"  : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmadc.vi"  : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsbc.vvm"  : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsbc.vxm"  : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmsbc.vvm" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmsbc.vxm" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmsbc.vv"  : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmsbc.vx"  : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},

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

# Vector Integer Arithmetic Instructions: Vector Single-Width Shift Instructions
    "vsll.vv" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsll.vx" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsll.vi" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsrl.vv" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsrl.vx" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsrl.vi" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsra.vv" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsra.vx" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsra.vi" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},

# Vector Integer Arithmetic Instructions: Vector Narrowing Integer Right Shift Instructions
    "vnsrl.wv" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vnsrl.wx" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vnsrl.wi" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vnsra.wv" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vnsra.wx" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vnsra.wi" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},

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

# Vector Integer Arithmetic Instructions: Vector Single-Width Integer Multiply-Add Instructions
     "vmacc.vv"  : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC", "latency" : 3},
     "vmacc.vx"  : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC", "latency" : 3},
     "vnmsac.vv" : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC", "latency" : 3},
     "vnmsac.vx" : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC", "latency" : 3},
     "vmadd.vv"  : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC", "latency" : 3},
     "vmadd.vx"  : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC", "latency" : 3},
     "vnmsub.vv" : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC", "latency" : 3},
     "vnmsub.vx" : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC", "latency" : 3},

# Vector Integer Arithmetic Instructions: Vector Widening Integer Multiply-Add Instructions
     "vwmaccu.vv"  : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC_WIDE_DEST", "latency" : 3},
     "vwmaccu.vx"  : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC_WIDE_DEST", "latency" : 3},
     "vwmacc.vv"   : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC_WIDE_DEST", "latency" : 3},
     "vwmacc.vx"   : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC_WIDE_DEST", "latency" : 3},
     "vwmaccsu.vv" : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC_WIDE_DEST", "latency" : 3},
     "vwmaccsu.vx" : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC_WIDE_DEST", "latency" : 3},
     "vwmaccus.vx" : {"pipe" : "vmul", "uop_gen" : "ARITH_MAC_WIDE_DEST", "latency" : 3},

# Vector Integer Arithmetic Instructions: Vector Integer Merge Instructions
# FIXME: Requires Mavis fix to include vector mask
    "vmerge.vvm" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmerge.vxm" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmerge.vim" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},

# Vector Integer Arithmetic Instructions: Vector Integer Move Instructions
    "vmv.v.v" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmv.v.x" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vmv.v.i" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},

# Vector Fixed-Point Arithmetic Instructions: Vector Single-Width Saturating Add and Subtract
    "vsaddu.vv" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsaddu.vx" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsaddu.vi" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},

    "vsadd.vv" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsadd.vx" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vsadd.vi" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    
    "vssubu.vv" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vssubu.vx" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    
    "vssub.vv" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},
    "vssub.vx" : {"pipe" : "vint", "uop_gen" : "ARITH", "latency" : 1},

# TODO: Vector Fixed-Point Arithmetic Instructions: Vector Single-Width Averaging Add and Subtract
# Vector Fixed-Point Arithmetic Instructions: Vector Single-Width Fractional Multiply with Rounding and Saturation
    "vsmul.vx" :   {"pipe" : "vmul", "uop_gen" : "ARITH", "latency" : 3},
    "vsmul.vv" :   {"pipe" : "vmul", "uop_gen" : "ARITH", "latency" : 3},

# TODO: Vector Fixed-Point Arithmetic Instructions: Vector Single-Width Scaling Shift Instructions
# TODO: Vector Fixed-Point Arithmetic Instructions: Vector Narrowing Fixed-Point Clip Instructions

# TODO: Vector Floating-Point Instructions: Vector Floating-Point Exception Flags
# Vector Floating-Point Instructions: Vector Single-Width Floating-Point Add/Subtract Instructions
    "vfadd.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfadd.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    
    "vfsub.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfsub.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfrsub.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

# Vector Floating-Point Instructions: Vector Widening Floating-Point Add/Subtract Instructions
    "vfwadd.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfwadd.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfwsub.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfwsub.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfwadd.wv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfwadd.wf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfwsub.wv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfwsub.wf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

# Vector Floating-Point Instructions: Vector Single-Width Floating-Point Multiply/Divide Instructions
    "vfmul.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfmul.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfdiv.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 25},
    "vfdiv.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 25},

    "vfrdiv.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" :25},

# Vector Floating-Point Instructions: Vector Widening Floating-Point Multiply
    "vfwmul.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},
    "vfwmul.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},

# Vector Floating-Point Instructions: Vector Single-Width Floating-Point Fused Multiply-Add Instructions
    "vfmacc.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfmacc.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfnmacc.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfnmacc.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfmsac.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfmsac.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfnmsac.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfnmsac.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfmadd.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfmadd.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfnmadd.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfnmadd.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfmsub.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfmsub.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfnmsub.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfnmsub.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

# Vector Floating-Point Instructions: Vector Widening Floating-Point Fused Multiply-Add Instructions
    "vfwmacc.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfwmacc.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfwnmacc.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfwnmacc.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfwmsac.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfwmsac.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfwnmsac.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfwnmsac.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

# Vector Floating-Point Instructions: Vector Floating-Point Square-Root Instruction
    "vfsqrt.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 25},

# TODO: support variable length latency
# Vector Floating-Point Instructions: Vector Floating-Point Reciprocal Square-Root Estimate Instruction
    "vfrsqrt7.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 2},

# Vector Floating-Point Instructions: Vector Floating-Point Reciprocal Estimate Instruction
    "vfrec7.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 2},

# Vector Floating-Point Instructions: Vector Floating-Point MIN/MAX Instructions
    "vfmin.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 1},
    "vfmin.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 1},

    "vfmax.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 1},
    "vfmax.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 1},

# Vector Floating-Point Instructions: Vector Floating-Point Sign-Injection Instructions
    "vfsgnj.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 1},
    "vfsgnj.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 1},

    "vfsgnjn.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 1},
    "vfsgnjn.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 1},

    "vfsgnjx.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 1},
    "vfsgnjx.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 1},

# Vector Floating-Point Instructions: Vector Floating-Point Compare Instructions
    "vmfeq.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 2},
    "vmfeq.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 2},

    "vmfne.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 2},
    "vmfne.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 2},

    "vmflt.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 2},
    "vmflt.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 2},

    "vmfle.vv" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 2},
    "vmfle.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 2},

    "vmfgt.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 2},

    "vmfgte.vf" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 2},

# Vector Floating-Point Instructions: Vector Floating-Point Classify Instruction
    "vfclass.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 1},

# Vector Floating-Point Instructions: Vector Floating-Point Merge Instruction
    "vfmerge.vfm" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 1},

# Vector Floating-Point Instructions: Vector Floating-Point Move Instruction
    "vfmv.v.f" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 1},

# Vector Floating-Point Instructions: Single-Width Floating-Point/Integer Type-Convert Instructions
    "vfcvt.xu.f.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},
    "vfcvt.x.f.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},

    "vfcvt.rtz.xu.f.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},
    "vfcvt.rtz.x.f.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},

    "vfcvt.f.xu.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},
    "vfcvt.f.x.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},

# Vector Floating-Point Instructions: Widening Floating-Point/Integer Type-Convert Instructions
    "vfwcvt.xu.f.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfwcvt.x.f.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfwcvt.rtz.xu.f.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfwcvt.rtz.x.f.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfwcvt.f.xu.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},
    "vfwcvt.f.xu.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

    "vfwcvt.f.f.v" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 6},

# Vector Floating-Point Instructions: Narrowing Floating-Point/Integer Type-Convert Instructions
    "vfncvt.xu.f.w" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},
    "vfncvt.x.f.w" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},

    "vfncvt.rtz.xu.f.w" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},
    "vfncvt.rtz.x.f.w" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},

    "vfncvt.f.xu.w" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},
    "vfncvt.f.x.w" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},

    "vfncvt.f.f.w" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},
    "vfncvt.rod.f.f.w" :  {"pipe" : "vfloat", "uop_gen" : "ARITH", "latency" : 3},

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
