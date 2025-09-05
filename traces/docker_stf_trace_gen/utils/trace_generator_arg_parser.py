import argparse
import sys
from data.consts import Const


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate traces for a workload",
        usage='python generate_trace.py [OPTIONS] WORKLOAD_PATH'
    )
    parser.add_argument("--emulator", required=True, choices=["spike", "qemu"])
    parser.add_argument("--isa", required=False, help="Instruction set architecture")
    parser.add_argument("--dump", action='store_true', required=False, default=False, help="Create trace file dump")
    parser.add_argument("--pk", action='store_true', required=False, default=False, help="Use Spike pk (proxy kernel)")
    parser.add_argument(
        "--image-name",
        required=False,
        default=Const.DOCKER_IMAGE_NAME,
        help=f"Custom docker image name. default: {Const.DOCKER_IMAGE_NAME}")
    parser.add_argument('-o', '--output', required=False, help='Output folder or file path')

    subparsers = parser.add_subparsers(title='Mode', dest='mode')
    subparsers.add_parser(
        'macro',
        help='Trace mode using START_TRACE and STOP_TRACE macros on the workload binary',
        description='Trace mode using START_TRACE and STOP_TRACE macros on the workload binary',
        formatter_class=argparse.RawTextHelpFormatter
    )

    inst_count_mode_parser = subparsers.add_parser(
        'insn_count',
        help='Traces a fixed number of instructions, after a given start instruction index',
        description='Traces a fixed number of instructions, after a given start instruction index',
        formatter_class=argparse.RawTextHelpFormatter
    )
    inst_count_mode_parser.add_argument(
        "--num-instructions",
        required=True,
        type=int,
        help="Number of instructions to trace")
    inst_count_mode_parser.add_argument(
        "--start-instruction",
        required=True,
        type=int,
        default=0,
        help="Number of instructions to skip before tracing (insn_count mode)")

    pc_mode_parser = subparsers.add_parser(
        'pc_count',
        help='Traces a fixed number of instructions, after a given PC value and PC hits count',
        description='Traces a fixed number of instructions, after a given PC value and PC hits count',
        formatter_class=argparse.RawTextHelpFormatter
    )
    pc_mode_parser.add_argument("--num-instructions", required=True, type=int, help="Number of instructions to trace")
    pc_mode_parser.add_argument("--start-pc", required=True, type=int, help="Starting program counter (pc_count mode)")
    pc_mode_parser.add_argument(
        "--pc-threshold",
        required=True,
        type=int,
        default=1,
        help="PC hit threshold (pc_count mode)")

    parser.add_argument("workload", help="Path to workload file")

    if len(sys.argv) == 1:
        parser.print_help()
        print("\nRun 'trace_share COMMAND --help' for more information on a command.")
        print("\nFor more help on how to use trace_share, head to GITHUB_README_LINK")
        sys.exit(0)

    args = parser.parse_args()
    return args
