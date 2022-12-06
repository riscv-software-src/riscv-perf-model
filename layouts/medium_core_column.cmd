###########################################################################
# core_column.cmd
#
# This file shows one column of the core layout and is intended to be included from core.cmd.
#

#-------------------------------------------------- Decode
pipem	"Decode"	decode.FetchQueue.FetchQueue				9	-1	4
pipeis	"Decode"	decode.FetchQueue.FetchQueue				3	-1	0

#-------------------------------------------------- Rename
pipem	"Rename"	rename.rename_uop_queue.rename_uop_queue	9	-1	4
pipeis	"Rename"	rename.rename_uop_queue.rename_uop_queue	3	-1	0

#-------------------------------------------------- Dispatch
pipem	"Dispatch"	dispatch.dispatch_queue.dispatch_queue		9	-1	4
pipeis	"Dispatch"	dispatch.dispatch_queue.dispatch_queue		3	-1	0

pipec	"FPU0 cred"	dispatch.in_fpu0_credits
pipec	"FPU1 cred"	dispatch.in_fpu1_credits
pipec	"ALU0 cred"	dispatch.in_alu0_credits
pipec	"ALU1 cred"	dispatch.in_alu1_credits
pipec	"ALU2 cred"	dispatch.in_alu2_credits
pipec	"BR0  cred"	dispatch.in_br0_credits
pipec	"LSU  cred"	dispatch.in_lsu_credits
pipec	"ROB  cred"	dispatch.in_reorder_buffer_credits
pipecs	"ROB flush"	dispatch.in_reorder_flush

#-------------------------------------------------- ALU0
pipem	"ALU0 Sch"	execute.alu0.scheduler_queue.scheduler_queue		7	-1	4
pipeis	"ALU0 Sch"	execute.alu0.scheduler_queue.scheduler_queue		3	-1	0

pipem	"ALU1 Sch"	execute.alu1.scheduler_queue.scheduler_queue		7	-1	4
pipeis	"ALU1 Sch"	execute.alu1.scheduler_queue.scheduler_queue		3	-1	0

pipem	"ALU2 Sch"	execute.alu2.scheduler_queue.scheduler_queue		7	-1	4
pipeis	"ALU2 Sch"	execute.alu2.scheduler_queue.scheduler_queue		3	-1	0

#-------------------------------------------------- FPU
pipem	"FPU0 Sch"	execute.fpu0.scheduler_queue.scheduler_queue			7	-1	4
pipeis	"FPU0 Sch"	execute.fpu0.scheduler_queue.scheduler_queue			3	-1	0

pipem	"FPU1 Sch"	execute.fpu1.scheduler_queue.scheduler_queue			7	-1	4
pipeis	"FPU1 Sch"	execute.fpu1.scheduler_queue.scheduler_queue			3	-1	0

#-------------------------------------------------- BR0
pipem	"BR0 Sch"	execute.br0.scheduler_queue.scheduler_queue			7	-1	4
pipeis	"BR0 Sch"	execute.br0.scheduler_queue.scheduler_queue			3	-1	0

#-------------------------------------------------- LSU
pipecs	"DL1 busy"	lsu.dcache_busy
pipeis	"LSU Pipe"	lsu.LoadStorePipeline.LoadStorePipeline		0	2

pipem	"LSU IQ"	lsu.lsu_inst_queue.lsu_inst_queue			7	-1	4
pipeis	"LSU IQ"	lsu.lsu_inst_queue.lsu_inst_queue			3	-1	0

#-------------------------------------------------- Retire
pipem	"ROB"		rob.ReorderBuffer.ReorderBuffer				29	-1	8
pipeis	"ROB"		rob.ReorderBuffer.ReorderBuffer				7	-1	0
