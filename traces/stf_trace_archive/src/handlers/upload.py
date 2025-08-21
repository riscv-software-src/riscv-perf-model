import argparse
import os
from tkinter.filedialog import FileDialog
from data.database_explorer import DatabaseExplorer
from data.consts import Const
from .base import CommandHandler
from utils.metadata_parser import MetadataParser
from utils.file_dialog import FileDialog
from utils.sha256 import compute_sha256
from utils.ui import print_metadata_interval
from data.trace_data import TraceData

class UploadHandler(CommandHandler):
    _metadata_file_suffix = ".metadata.yaml"

    def run(self, args: argparse.Namespace, database_explorer: DatabaseExplorer):
        self.explorer = database_explorer
        traces = self._get_arg_traces(args)
        self._validate_traces(traces)
        workload_id = self._get_workload_id(traces, args)
        self._setup_trace_attempt(traces, workload_id)
        self._setup_trace_parts(traces, workload_id)
        self._setup_trace_ids(traces, workload_id)
        self.explorer.upload_traces(traces)
        
    def _get_arg_traces(self, args) -> list[TraceData]:
        trace_paths = args.trace

        if(trace_paths and isinstance(trace_paths, str)):
            trace_paths = [trace_paths]
        
        if args.it:
            if not args.trace:
                trace_paths = FileDialog.select_traces()
                args.trace = trace_paths

        traces = []
        for trace_path in trace_paths:
            metadata_path = trace_path + self._metadata_file_suffix
            if not os.path.exists(metadata_path):
                raise FileNotFoundError(f"Metadata file not found: {metadata_path}")
            
            metadata = MetadataParser.parse_metadata_from_path(metadata_path)
            traces.append(TraceData(trace_path=trace_path, metadata_path=metadata_path, metadata=metadata))

        if len(traces) <= 0:
            raise ValueError("No traces provided.")
        
        return traces
    
    def _validate_traces(self, traces):        
        workload_sha256 = traces[0].metadata['workload']['SHA256']
        for trace in traces:
            if trace.metadata['workload']['SHA256'] != workload_sha256:
                raise ValueError("Traces from different workloads provided. Please provide traces from the same workload.")

        if len(traces) > 1:
            for trace in traces:
                if MetadataParser.is_fully_traced(trace.metadata):
                    raise ValueError("Multiple fully traced traces provided. Please provide only one fully traced trace or multiple partial traces.")     

    def _setup_trace_attempt(self, traces, workload_id):
        trace_attempt = self._get_trace_attempt(traces, workload_id)
        for trace in traces:
            trace.attempt = trace_attempt

    def _get_trace_attempt(self, traces, workload_id):
        fully_traced = len(traces) == 1 and MetadataParser.is_fully_traced(traces[0].metadata)
        if fully_traced:
            return self.explorer.get_next_trace_attempt(workload_id)
        
        used_trace_attempts = self.explorer.get_trace_attempts(workload_id)
        if len(used_trace_attempts) == 0:
            return 0

        print("Do you with to upload to a new trace attempt? (yes/no): ", end="")
        answer = input().lower()
        if answer != 'yes' and answer != 'y' and answer != 'no' and answer != 'n':
            raise ValueError("Invalid response. Please answer 'yes' or 'no'.")
        
        if answer == 'yes' or answer == 'y':
            return self.explorer.get_next_trace_attempt(workload_id)
        
        print(f"Existing trace attempts: {used_trace_attempts}")
        print("Please provide the trace attempt number to upload to: ", end="")
        trace_attempt = input().strip()
        if not trace_attempt.isdigit() or int(trace_attempt) not in used_trace_attempts:
            raise ValueError(f"Trace attempt value must be a number between {min(used_trace_attempts)} and {max(used_trace_attempts)}")
        
        trace_attempt = int(trace_attempt)
        if self.explorer.is_fully_traced(workload_id, trace_attempt):
            raise ValueError(f"Trace attempt {trace_attempt} for workload ID {workload_id} is fully traced. Cannot upload more traces to a fully traced attempt.")

        return trace_attempt

    def _setup_trace_parts(self, traces, workload_id):
        if len(traces) == 1 and MetadataParser.is_fully_traced(traces[0].metadata):
            for trace in traces:
                trace.part = 0
            return traces
    
        print("Partial traces detected. Please specify the part number for this trace segment")
        traces = sorted(traces, key = lambda trace: trace.metadata['stf']['trace_interval']['start_instruction_index'])

        trace_attempt = traces[0].attempt
        used_parts = self.explorer.get_trace_parts(workload_id, trace_attempt)
        print(f"used_parts: {used_parts}")
        last_part_number = max(used_parts) if used_parts else -1
        for trace in traces:
            print(f"\nTrace file: {trace.path}")
            print_metadata_interval(trace.metadata)

            default_option = last_part_number + 1
            print(f"Part number [{format(default_option)}]: ", end="")
            part_number = input()
            if part_number == "":
                part_number = default_option
            else:
                part_number = int(part_number)

            if part_number in used_parts:
                raise ValueError(f"Part number {part_number} already used. Please use unique part numbers for each trace segment.")
            
            used_parts.append(part_number)
            trace.part = part_number
            last_part_number = part_number

        return traces

    def _get_workload_id(self, traces, args):
        workload_sha256 = traces[0].metadata['workload']['SHA256']

        workload_id = self.explorer.get_workload_id(workload_sha256)
        if workload_id is not None:
            print("Workload already exists in trace archive, skipping workload upload.")
            return workload_id
            
        if not args.it and not args.workload:
            raise ValueError("Workload not found on trace archive. Provide ither --it or --workload options to specify the workload binary.")
        
        workload_path = args.workload
        if args.it and not args.workload:
            workload_path = FileDialog.select_workload()

        if not workload_path or not os.path.exists(workload_path):
            raise FileNotFoundError(f"Workload file not found: {workload_path}")
        
        workload_file_sha256 = compute_sha256(workload_path)
        if workload_file_sha256 != workload_sha256:
            raise ValueError("Workload file SHA256 does not match the one in metadata.")
        
        workload_id = self.explorer.upload_workload(workload_path)
        return workload_id    

    def _setup_trace_ids(self, traces, workload_id):
        for trace in traces:
            workload_name = self.explorer.get_workload_name(workload_id)
            
            if not workload_name:
                raise ValueError(f"Workload with ID {workload_id} not found in the database.")
            
            trace_part = str(trace.part).zfill(Const.PAD_LENGHT)
            trace_id = f"{workload_id}.{trace.attempt}.{trace_part}_{workload_name}"
            trace.id = trace_id
            trace.metadata['trace_id'] = trace_id
            trace.metadata['workload']['filename'] = workload_name
        
        return traces