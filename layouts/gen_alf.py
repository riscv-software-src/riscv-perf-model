#!/bin/env python3

import sys

sys.path.append('../../map/helios/pipeViewer/scripts/')

from alf_gen.ALFLayout import ALFLayout
import argparse
import sys

parser = argparse.ArgumentParser(prog=f'{sys.argv[0]}',
                                 description='Generate an ALF file with a given locations.dat file',
                                 formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=f"""
Script to auto-generate ALF files.
""")

parser.add_argument('-d', '--database', type=str, nargs=1, help="The pipeout database")
parser.add_argument('-a', '--alf', type=str, nargs=1, help="The name of the ALF file to generate")
parser.add_argument('-n', '--num-cycles', nargs=1, type=int, help="Number of cycles to generate", default=80)

args = parser.parse_args()
pipeout_location = args.database
if pipeout_location is None:
    print("ERROR: Where's the pipeout locations file? (-d location.dat)")
    exit(1)
else:
    pipeout_location = pipeout_location[0]

alf_file = args.alf
if alf_file is None:
    print("ERROR: Where do you want the ALF to be written to? (-a output.alf)")
    exit(1)
else:
    alf_file = alf_file[0]
    if not alf_file.endswith('.alf'):
        alf_file += '.alf'

num_cycles = args.num_cycles if isinstance(args.num_cycles, int) else args.num_cycles[0]

general_height = 15
layout = ALFLayout(start_time  = -10,
                   num_cycles  = num_cycles,
                   clock_scale = 1,
                   location_file=pipeout_location,
                   alf_file=alf_file)

layout.setSpacing(ALFLayout.Spacing(height         = general_height,
                                    height_spacing = general_height - 1,
                                    melem_height   = general_height/3,
                                    melem_spacing  = general_height/3 - 1,
                                    caption_width  = 150,
                                    onechar_width  = 9))

sl_grp = layout.createScheduleLineGroup(default_color=[192,192,192],
                                        include_detail_column = True,
                                        margins=ALFLayout.Margin(top = 2, left = 10))

#-------------------------------------------------- Fetch/Decode
sl_grp.addScheduleLine('.*decode.FetchQueue.FetchQueue', ["Decode"], mini_split=[80,20], space=True)

#-------------------------------------------------- Rename
sl_grp.addScheduleLine('.*rename.rename_uop_queue.rename_uop_queue', ["Rename"], mini_split=[80,20], space=True)

#-------------------------------------------------- Dispatch
sl_grp.addScheduleLine('.*dispatch.dispatch_queue.dispatch_queue', ["Dispatch"], mini_split=[80,20], space=True)

sl_grp.addScheduleLine('.*dispatch.in_alu([0-9])_credits', [r"ALU[\1] cred"], nomunge=True)
sl_grp.addScheduleLine('.*dispatch.in_fpu([0-9])_credits', [r"FPU[\1] cred"], nomunge=True)

sl_grp.addScheduleLine('.*dispatch.in_br([0-9])_credits',      [r"BR[\1] cred"], nomunge=True)
sl_grp.addScheduleLine('.*dispatch.in_lsu_credits',            ["LSU  cred"], nomunge=True)
sl_grp.addScheduleLine('.*dispatch.in_reorder_buffer_credits', ["ROB  cred"], nomunge=True, space=True)

#-------------------------------------------------- Exe Blocks
for block in ['alu','fpu','br']:
    num_blocks = layout.count(f'(.*{block}[0-9]+)\..*')
    num = 0
    while num < num_blocks:
        sl_grp.addScheduleLine(f'.*execute.{block}{num}.scheduler_queue.scheduler_queue([0-9]+)',
                               [rf"{block}{num} Sch[\1]"], mini_split=[80,20], space=True)
        num += 1

#-------------------------------------------------- LSU
sl_grp.addScheduleLine('.*lsu.lsu_inst_queue.lsu_inst_queue', ["LSU IQ[\1]"], mini_split=[80,20])
sl_grp.addScheduleLine('.*lsu.replay_buffer.replay_buffer([0-9]+)', ["LSU Replay[\1]"], mini_split=[80,20])

sl_grp.addScheduleLine('.*lsu.LoadStorePipeline.LoadStorePipeline', ["LSU Pipe"], space=True, reverse=False)
sl_grp.addScheduleLine('.*lsu.dcache_busy', ["DL1 busy"], nomunge=True)

#-------------------------------------------------- Retire
sl_grp.addScheduleLine('.*rob.ReorderBuffer.ReorderBuffer([0-9]+)', [r"ROB[\1]"], mini_split=[80,20], space=True)
sl_grp.addScheduleLine('.*dispatch.in_reorder_flush',               ["ROB flush"], nomunge=True)
sl_grp.addCycleLegend(ALFLayout.CycleLegend(location = '.*rob.ReorderBuffer.ReorderBuffer0',
                                            interval = 5))
