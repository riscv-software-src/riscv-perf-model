
from typing import Optional
from data.trace_table_shema import TracesTableSchema
from data.workload_table_shema import WorkloadsTableSchema
from data.storages.base import StorageHandler
from data.metadata import Metadata
from data.trace_data import TraceData


class DatabaseExplorer:
    def __init__(self, storage: StorageHandler):
        self.storage = storage

    def get_workload_id(self, workload_sha256: str) -> Optional[int]:
        workload_traces = self.storage.traces_table[self.storage.traces_table[
            TracesTableSchema.WORKLOAD_SHA] == workload_sha256]
        if len(workload_traces) == 0:
            return None

        return workload_traces.iloc[0][TracesTableSchema.WORKLOAD_ID]

    def upload_workload(self, workload_path: str) -> int:
        workload_id = self._get_next_workload_id()
        print(f"Uploading workload: {workload_path} with id: {workload_id}")
        self.storage.insert_workload(workload_path, workload_id)
        return workload_id

    def _get_next_workload_id(self) -> int:
        workload_ids = self.storage.traces_table[TracesTableSchema.WORKLOAD_ID].unique(
        )
        max_workload_id = max(workload_ids, default=-1)
        return max_workload_id + 1

    def get_workload_name(self, workload_id: int) -> Optional[str]:
        workload_row = self.storage.workloads_table[
            self.storage.workloads_table[WorkloadsTableSchema.WORKLOAD_ID] == workload_id]
        if len(workload_row) > 0:
            return workload_row.iloc[0][WorkloadsTableSchema.WORKLOAD_NAME]

        return None

    def get_next_trace_attempt(self, workload_id: int) -> int:
        trace_attemps = self.get_trace_attempts(workload_id)
        if not trace_attemps:
            return 0

        max_attempt = max(trace_attemps)
        return max_attempt + 1

    def get_trace_attempts(self, workload_id: int) -> list[int]:
        workload_traces = self.storage.traces_table[
            self.storage.traces_table[TracesTableSchema.WORKLOAD_ID] == workload_id]
        return workload_traces[TracesTableSchema.TRACE_ATTEMPT].unique().tolist()

    def get_trace_parts(self, workload_id: int, trace_attempt: int) -> list[int]:
        workload_traces = self.storage.traces_table[
            (self.storage.traces_table[TracesTableSchema.WORKLOAD_ID] == workload_id) &
            (self.storage.traces_table[TracesTableSchema.TRACE_ATTEMPT]
             == trace_attempt)
        ]

        return workload_traces[TracesTableSchema.TRACE_PART].unique().tolist()

    def check_trace_exists(self, trace_id: str) -> bool:
        return trace_id in self.storage.traces_table[TracesTableSchema.TRACE_ID].values

    def upload_traces(self, traces: list[TraceData]) -> None:
        self.storage.update_traces_table()
        for trace in traces:
            print(f"Uploading trace: {trace.path} with id: {trace.id}")
            self._upload_trace(trace.path, trace.metadata)

    def _upload_trace(self, trace_path: str, metadata: Metadata) -> None:
        trace_id = metadata.get('trace_id')
        if not trace_id:
            raise ValueError("Trace ID is required in metadata to upload a trace.")

        if self.check_trace_exists(trace_id):
            raise ValueError(f"Trace with ID {trace_id} already exists in the database.")

        self.storage.insert_trace(trace_path, metadata)

    def is_fully_traced(self, workload_id: int, trace_attempt: int) -> Optional[bool]:
        trace_row = self.storage.traces_table[
            (self.storage.traces_table[TracesTableSchema.WORKLOAD_ID] == workload_id) &
            (self.storage.traces_table[TracesTableSchema.TRACE_ATTEMPT]
             == trace_attempt)
        ]
        if len(trace_row) == 0:
            return None

        return trace_row.iloc[0][TracesTableSchema.FULLY_TRACED]

    def get_trace_ids(self) -> list[str]:
        return self.storage.traces_table[TracesTableSchema.TRACE_ID].to_list()

    def get_metadata(self, trace_id: str) -> Metadata:
        return self.storage.get_metadata(trace_id)

    def save_metadata(self, trace_id: str, path: str) -> None:
        return self.storage.save_metadata(trace_id, path)

    def save_trace(self, trace_id: str, path: str) -> None:
        return self.storage.save_trace(trace_id, path)

    def save_workload(self, workload_id: int, path: str) -> None:
        return self.storage.save_workload(workload_id, path)
