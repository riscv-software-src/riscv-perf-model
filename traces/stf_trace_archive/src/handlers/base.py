from abc import ABC, abstractmethod
import argparse

from database_explorer.database_explorer import DatabaseExplorer

class CommandHandler(ABC):
    @abstractmethod
    def run(self, args: argparse.Namespace, database_explorer: DatabaseExplorer) -> None:
        raise NotImplementedError("This method should be overridden by subclasses.")