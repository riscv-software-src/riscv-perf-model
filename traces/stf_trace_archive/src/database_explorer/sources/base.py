import pandas as pd

from abc import ABC, abstractmethod
from data.output_path import OutputPaths
from data.metadata import Metadata

class SourceHandler(ABC):
    @property
    def traces_table(self):
        raise NotImplementedError("This method should be overridden by subclasses.")
    
    @property
    def workloads_table(self):
        raise NotImplementedError("This method should be overridden by subclasses.")
    
    @abstractmethod
    def update_traces_table(self) -> pd.DataFrame:
        raise NotImplementedError("This method should be overridden by subclasses.")
    
    @abstractmethod
    def update_workloads_table(self) -> pd.DataFrame:
        raise NotImplementedError("This method should be overridden by subclasses.")

    @abstractmethod
    def insert_trace(self, trace_path: str, metadata: Metadata) -> None:
        raise NotImplementedError("This method should be overridden by subclasses.")

    @abstractmethod
    def insert_workload(self, workload_path: str, workload_sha: str) -> None:
        raise NotImplementedError("This method should be overridden by subclasses.")

    @abstractmethod
    def get_metadata(self, trace_id: str) -> Metadata:
        raise NotImplementedError("This method should be overridden by subclasses.")

    @abstractmethod
    def save_metadata(self, trace_id: str, path: OutputPaths) -> None:
        raise NotImplementedError("This method should be overridden by subclasses.")

    @abstractmethod
    def save_trace(self, trace_id: str, path: OutputPaths) -> None:
        raise NotImplementedError("This method should be overridden by subclasses.")

    @abstractmethod
    def save_workload(self, workload_id: int, path: OutputPaths) -> None:
        raise NotImplementedError("This method should be overridden by subclasses.")

    @staticmethod
    def setup():
        raise NotImplementedError("This method should be overridden by subclasses.")
    