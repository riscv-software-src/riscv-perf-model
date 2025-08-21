import os
import readline
import shutil
from typing import Optional, Union
import pandas as pd
import yaml

from data.workload_table_shema import WorkloadsTableSchema
from data.config import LocalStorageConfig
from data.consts import Const
from data.metadata import Metadata
from .base import SourceHandler
from data.trace_table_shema import TracesTableSchema
from utils.metadata_parser import MetadataParser

class LocalStorageSource(SourceHandler):
    def __init__(self, config: LocalStorageConfig):
        if not config.path:
            raise ValueError("Storage path cannot be empty.")

        self.storage_path = config.path
        self.trace_suffix = "zstf"
        self.metadata_suffix = "zstf.metadata.yaml"
        self.metadata_cache = {}
        self._traces_table = None
        self._workloads_table = None

    @property
    def traces_table(self):
        if self._traces_table is None:
            return self.update_traces_table()
        
        return self._traces_table

    @property
    def workloads_table(self):
        if self._workloads_table is None:
            return self.update_workloads_table()
        
        return self._workloads_table
    
    def update_traces_table(self) -> pd.DataFrame:
        df = pd.DataFrame(columns=TracesTableSchema.get_columns())
        if not os.path.exists(self.storage_path):
            os.mkdir(self.storage_path)
        
        self.update_workloads_table()
        workload_ids = self.workloads_table[WorkloadsTableSchema.WORKLOAD_ID].to_list()
        for workload_id in workload_ids:
            workload_folder = self._get_workload_folder(workload_id)
            workload_path = os.path.join(self.storage_path, workload_folder)
            trace_attempts = os.listdir(workload_path)

            for trace_attempt in trace_attempts:
                trace_attempt_path = os.path.join(workload_path, trace_attempt)
                if not os.path.isdir(trace_attempt_path):
                    continue

                trace_files = os.listdir(trace_attempt_path)
                metadata_files = [filename for filename in trace_files if filename.endswith(self.metadata_suffix)]
                trace_ids = [metadata_file[0:-len(self.metadata_suffix) - 1] for metadata_file in metadata_files]
                trace_attemps_id = int(trace_attempt.split('_', 1)[1])

                if len(trace_ids) == 0:
                    continue

                sample_metadata = self.get_metadata(trace_ids[0])
                sample_trace_interval = sample_metadata.get('stf', {}).get('trace_interval', None)
                fully_trace = True if sample_trace_interval is None else False
                workload_sha = sample_metadata.get('workload', {}).get('SHA256', None)

                for trace_id in trace_ids:
                    trace_part = trace_id.split('.')[1]
                    workload_name = '_'.join(trace_id.split('_')[1:])
                    df = pd.concat([df, pd.DataFrame([{
                        TracesTableSchema.TRACE_ID: trace_id,
                        TracesTableSchema.TRACE_ATTEMPT: trace_attemps_id,
                        TracesTableSchema.TRACE_PART: int(trace_part),
                        TracesTableSchema.WORKLOAD_ID: workload_id,
                        TracesTableSchema.WORKLOAD_SHA: workload_sha,
                        TracesTableSchema.WORKLOAD_NAME: workload_name,
                        TracesTableSchema.FULLY_TRACED: fully_trace
                    }])])
        
        self._traces_table = df
        return df
    
    def update_workloads_table(self) -> pd.DataFrame:
        df = pd.DataFrame(columns=WorkloadsTableSchema.get_columns())
        if not os.path.exists(self.storage_path):
            os.mkdir(self.storage_path)
            
        workload_folders = os.listdir(self.storage_path)
        for workload_folder in workload_folders:
            workload_id, workload_name = workload_folder.split('_', 1)
            df = pd.concat([df, pd.DataFrame([{
                WorkloadsTableSchema.WORKLOAD_ID: int(workload_id),
                WorkloadsTableSchema.WORKLOAD_NAME: workload_name,
            }])])

        self._workloads_table = df
        return df

    def insert_workload(self, workload_path: str, workload_id: int) -> None:
        workload_name = os.path.basename(workload_path)
        workload_folder = self._get_workload_folder(workload_id, workload_name)
        storage_path = os.path.join(self.storage_path, workload_folder)
        os.makedirs(storage_path, exist_ok=False)
        shutil.copy(workload_path, storage_path)       

        self._workloads_table = pd.concat([self.workloads_table, pd.DataFrame([{
            WorkloadsTableSchema.WORKLOAD_ID: int(workload_id),
            WorkloadsTableSchema.WORKLOAD_NAME: workload_name,
        }])])

    def insert_trace(self, trace_path: str, metadata: Metadata) -> None:
        trace_id = metadata.get('trace_id')
        if not trace_id:
            raise ValueError("Trace ID is required in metadata to insert a trace.")

        workload_id, trace_attempt = trace_id.split('.')[:2]
        workload_folder = self._get_workload_folder(workload_id)
        attempt_folder = self._get_attempt_folder(trace_attempt)

        storage_path = os.path.join(self.storage_path, workload_folder, attempt_folder)
        trace_storage_path = os.path.join(storage_path, f"{trace_id}.{self.trace_suffix}")
        metadata_storage_path = os.path.join(storage_path, f"{trace_id}.{self.metadata_suffix}")

        os.makedirs(storage_path, exist_ok=True)
        shutil.copy(trace_path, trace_storage_path)
        with open(metadata_storage_path, 'w') as metadata_file:
            yaml.dump(metadata, metadata_file)

    def get_metadata(self, trace_id: str) -> Metadata:
        if trace_id in self.metadata_cache:
            return self.metadata_cache[trace_id]
    
        workload_id, trace_attempt = trace_id.split('.')[:2]
        workload_folder = self._get_workload_folder(workload_id)
        attempt_folder = self._get_attempt_folder(trace_attempt)
        metadata_path = os.path.join(self.storage_path, workload_folder, attempt_folder, f"{trace_id}.{self.metadata_suffix}")
        if not os.path.exists(metadata_path):
            raise FileNotFoundError(f"Metadata file not found: {trace_id}")
                
        metadata = MetadataParser.parse_metadata_from_path(metadata_path)
        self.metadata_cache[trace_id] = metadata
        return metadata

    def save_metadata(self, trace_id: str, path: str) -> None:
        metadata_filename = f"{trace_id}.{self.metadata_suffix}"
        dst_filename = path.filename if path.filename else metadata_filename
        dst_path = os.path.join(path.folder_path, dst_filename)

        metadata = self.get_metadata(trace_id)
        with open(dst_path, "w") as metadata_file:
            yaml.dump(metadata, metadata_file)
        print(f"Metadata {trace_id} saved on {os.path.abspath(dst_path)}")

    def save_trace(self, trace_id: str, path: str) -> None:
        workload_id, trace_attempt = trace_id.split('.')[:2]
        workload_folder = self._get_workload_folder(workload_id)
        attempt_folder = self._get_attempt_folder(trace_attempt)
        trace_filename = f"{trace_id}.{self.trace_suffix}"
        trace_path = os.path.join(self.storage_path, workload_folder, attempt_folder, trace_filename)

        dst_filename = path.filename if path.filename else trace_filename
        dst_path = os.path.join(path.folder_path, dst_filename)
        shutil.copy(trace_path, dst_path)
        print(f"Trace {trace_id} saved on {os.path.abspath(dst_path)}")

    def save_workload(self, workload_id: int, path: str) -> None:
        workload_folder = os.path.join(self.storage_path, self._get_workload_folder(workload_id))
        if not os.path.exists(workload_folder):
            raise FileNotFoundError(f"Workload not found: {workload_id}")
        
        workload_path_list = os.listdir(workload_folder)
        workload_filename = None
        workload_file_path = None
        for workload_file in workload_path_list:
            full_path = os.path.join(workload_folder, workload_file)
            if not os.path.isfile(full_path):
                continue

            if workload_file_path is not None:
                raise NotImplementedError("Multiple workload files found.")
            
            workload_filename = workload_file
            workload_file_path = full_path
        
        dst_filename = path.filename if path.filename else workload_filename
        dst_path = os.path.join(path.folder_path, dst_filename)
        shutil.copy(workload_file_path, dst_path)
        print(f"Workload {workload_id} saved on {os.path.abspath(dst_path)}")
    
    def _get_workload_folder(self, workload_id: Union[str, int], workload_name: Optional[str] = None) -> str:
        if isinstance(workload_id, str):
            workload_id = int(workload_id)

        if not workload_name:
            workload_name = self.workloads_table[self.workloads_table[WorkloadsTableSchema.WORKLOAD_ID] == workload_id][WorkloadsTableSchema.WORKLOAD_NAME].item()

        workload_folder = f"{str(workload_id).zfill(Const.PAD_LENGHT)}_{workload_name}"
        return workload_folder
    
    def _get_attempt_folder(self, attempt_id: Union[str, int]) -> str:
        return f"attempt_{str(attempt_id).zfill(Const.PAD_LENGHT)}"
    
    @staticmethod
    def setup() -> LocalStorageConfig:
        readline.set_completer_delims(' \t\n=')
        readline.parse_and_bind("tab: complete")
        path = input("Enter the storage folder path: ").lower()
        readline.parse_and_bind("tab: self-insert")
        path = os.path.abspath(path)

        return LocalStorageConfig(path=path)