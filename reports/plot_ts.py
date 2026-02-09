#!/usr/bin/env python3

import sys
import pandas as pd
import matplotlib.pyplot as plt

csv_names = []

# Uncomment for comparing more than 1 csv file
#if len(sys.argv) < 3:
#    print("Missing csv files");
#    exit(1)

csv_names.extend(sys.argv[1:])

wkld_limits = []
if len(sys.argv) == 5:
    wkld_limits = [int(sys.argv[3]), int(sys.argv[4])]

plot_items = [
    "top.cpu.core0.rob.ipc",
    "top.cpu.core0.dispatch.stall_not_stalled",
    "top.cpu.core0.dispatch.stall_no_rob_credits",
    "top.cpu.core0.dispatch.stall_alu_busy",
    "top.cpu.core0.dispatch.stall_fpu_busy",
    "top.cpu.core0.dispatch.stall_lsu_busy",
    "top.cpu.core0.dispatch.stall_br_busy",
]

axes_plots = []

for csv in csv_names:
    print("Reading ", csv)
    # Create a time series with total number of insts retired being the index column (the x-axis)
    ts = pd.read_csv(csv, header=2, index_col="top.cpu.core0.rob.total_number_retired")

    # The graphs are based on the Insts column, but the inst column is not
    # cumulative.  Modify the data to be.
    index_vals = ts.index.values
    new_index_vals = []
    idx_point = 0
    for iv in index_vals:
        new_index_vals.append(iv + idx_point)
        idx_point += iv
    ts.set_index(pd.Index(new_index_vals), inplace=True)

    # Plot it
    axes_plots.append(ts[plot_items].plot(kind="line", title=csv, figsize=(15,12), subplots=True))

if len(wkld_limits) > 0:
    for axes in axes_plots:
        for ax in axes:
            ax.set_xlim(wkld_limits)

plt.show()
