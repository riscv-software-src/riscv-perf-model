import argparse
from database_explorer.database_explorer import DatabaseExplorer
from .base import CommandHandler

# TODO
class SearchHandler(CommandHandler):
    def run(self, args: argparse.Namespace, database_explorer: DatabaseExplorer):
        raise NotImplementedError("SearchHandler is not implemented yet.")