import argparse
import os
from data.database_explorer import DatabaseExplorer
from data.output_path import OutputPaths
from .base import CommandHandler


class GetHandler(CommandHandler):
    def run(self, args: argparse.Namespace, database_explorer: DatabaseExplorer) -> None:
        self.explorer = database_explorer
        output_path: OutputPaths = self._get_output_path(args.output)
        if args.trace is not None:
            self._save_trace(args.trace, output_path)

        elif args.workload is not None:
            self.explorer.save_workload(args.workload, output_path)

        elif args.metadata is not None:
            self.explorer.save_metadata(args.metadata, output_path)

        else:
            raise ValueError("Invalid arguments: expected one of --trace, --workload, or --metadata")

    def _get_output_path(self, output_arg: str) -> OutputPaths:
        if output_arg is None:
            return OutputPaths(folder_path="./", filename=None)

        folder: str = os.path.dirname(output_arg)
        filename: str = os.path.basename(output_arg)
        return OutputPaths(folder_path=folder, filename=filename)

    def _save_trace(self, trace_id: str, output_path: str) -> None:
        self.explorer.save_trace(trace_id, output_path)
        if output_path.filename:
            output_path.filename += ".metadata.yaml"

        self.explorer.save_metadata(trace_id, output_path)
