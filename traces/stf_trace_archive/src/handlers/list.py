import argparse
from .base import CommandHandler
from data.database_explorer import DatabaseExplorer
from utils.ui import print_medatata_details


class ListHandler(CommandHandler):
    def run(
        self,
        args: argparse.Namespace,
        database_explorer: DatabaseExplorer
    ) -> None:
        self.explorer = database_explorer
        match vars(args):
            case _:
                self._list_traces()

    def _list_traces(self) -> None:
        trace_ids = self.explorer.get_trace_ids()
        if not trace_ids:
            print("No traces found.")
            return

        for trace_id in sorted(trace_ids):
            metadata = self.explorer.get_metadata(trace_id)
            print_medatata_details(metadata)
            print("")
