import argparse
import sys


def parseArgs() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog='trace_share',
        usage='python trace_share.py COMMAND [OPTIONS]',
        description='CLI tool for Olympia traces exploration',
        epilog="For more help on how to use trace_share, head to https://github.com/riscv-software-src/riscv-perf-model/tree/master/traces/stf_trace_archive/README.md",
        formatter_class=argparse.RawTextHelpFormatter,
        add_help=False
    )

    parser.add_argument('-h', '--help', action='help', help='Show this help message and exit.')
    parser.add_argument('--storage-name', help='Select a pre-configured storage config.')

    subparsers = parser.add_subparsers(title='Commands', dest='command')

    upload_parser = subparsers.add_parser(
        'upload',
        help='Upload workload and trace.',
        description='Upload a workload, trace and metadataz to the database. At least one of the options must be used',
        formatter_class=argparse.RawTextHelpFormatter
    )
    upload_parser.add_argument('--workload', help='(optional) Path to the workload file.')
    upload_parser.add_argument('--trace', action='append', help='Path to one or more trace files. If omitted, defaults to <workload>.zstf')
    upload_parser.add_argument('--it', action='store_true', help='Iteractive file selection mode.')

    list_parser = subparsers.add_parser(
        'list',
        help='List items by category.',
        description='List database traces or related entities.',
        formatter_class=argparse.RawTextHelpFormatter
    )
    group = list_parser.add_mutually_exclusive_group()
    group.add_argument('--traces', action='store_true', help='Lists available traces (default)')

    get_parser = subparsers.add_parser(
        'get',
        help='Download a specified trace or workload file.',
        description='Download a specified trace or workload file.',
        formatter_class=argparse.RawTextHelpFormatter
    )

    group = get_parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--trace', help='Id of the trace to download.', metavar='TRACE_ID')
    group.add_argument('--metadata', help='Id of the metadata (same as trace) to download.', metavar='TRACE_ID')
    group.add_argument('--workload', help='Id of the workload to download.', metavar='WORKLOAD_ID')
    get_parser.add_argument('-o', '--output', help='Output folder or file path')

    setup_parser = subparsers.add_parser(
        'setup',
        help='Create or edit current tool configurations',
        description='Create or edit current tool configurations',
        formatter_class=argparse.RawTextHelpFormatter
    )
    setup_parser.add_argument('--add-storage', action='store_true', help='Create a new storage source')
    setup_parser.add_argument('--set-default-storage', help='Select the default storage')

    if len(sys.argv) == 1:
        parser.print_help()
        print("\nRun 'trace_share COMMAND --help' for more information on a command.")
        print("\nFor more help on how to use trace_share, head to GITHUB_README_LINK")
        sys.exit(0)

    args = parser.parse_args()

    if args.command == 'upload':
        if not (args.workload or args.trace or args.it):
            upload_parser.print_help()
            print("\nError: At least one of --workload, --trace, or --it must be provided.")
            exit(1)

    return args
