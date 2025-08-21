from datetime import datetime
import hashlib
import os
import yaml

class TraceDataGenerator():
    def __init__(self):
        self.test_storage_path = "./tests/traces_test"
        os.makedirs(self.test_storage_path, exist_ok=True)

    def generate_worload(self, workload_id = None):
        if workload_id is None:
            workload_files = os.listdir(self.test_storage_path)
            workload_ids = [f.split('_')[1] for f in workload_files if f.startswith('workload_')]
            workload_id = int(len(workload_ids) + 1)
        
        workload_file_content = f"Workload ID: {workload_id}\n"
        workload_path = f"{self.test_storage_path}/workload_{workload_id}"

        with open(f"{workload_path}", 'w') as workload_file:
            workload_file.write(workload_file_content)

        return workload_path
    
    def generate_trace(self, workload_id, trace_attempt, trace_part = None):        
        trace_attempt_path = f"{self.test_storage_path}/trace_attempt_{trace_attempt}"
        os.makedirs(trace_attempt_path, exist_ok=True)

        if trace_part is None:
            trace_path = f"{trace_attempt_path}/0.zstf"
            metadata_path = f"{trace_attempt_path}/0.zstf.metadata.yaml"
        else:
            trace_path = f"{trace_attempt_path}/{trace_part}.zstf"
            metadata_path = f"{trace_attempt_path}/{trace_part}.zstf.metadata.yaml"

        trace_file_content = f"Trace attempt: {trace_attempt}, trace part: {trace_part}\n"
        with open(trace_path, 'w') as trace_file:
            trace_file.write(trace_file_content)

        trace_metadata_content = self.generate_metadata(workload_id, trace_part)
        with open(metadata_path, 'w') as metadata_file:
            yaml.dump(trace_metadata_content, metadata_file)

        return trace_path

    def generate_metadata(self, workload_id, trace_part):
        workload_sha256 = self.get_workload_sha256(workload_id)

        interval = None
        if trace_part is not None:
            interval = {
                'instruction_pc': 100 * trace_part,
                'pc_count': trace_part,
                'interval_lenght': trace_part * 100,
                'start_instruction_index': 100 * trace_part,
                'end_instruction_index': 100 * (trace_part + 1)
            }
        
        metadata = {
            'description': None,
            'author': {
                'name': 'Jane Doe',
                'company': 'RISCV',
                'email': 'jane.doe@riscv.org'
            },
            'workload': {
                'filename': f"{workload_id}",
                'SHA256': workload_sha256,
                'execution_command': f"./{workload_id}",
                'elf_sections': {
                    'comment': "Test",
                    'riscv.attributes': "Test",
                    'GCC.command.line': "Test"
                },
            },
            'stf': {
                'timestamp': datetime.now().isoformat(),
                'stf_trace_info': {
                    'VERSION': "Test",
                    'GENERATOR': "Test",
                    'GEN_VERSION': "Test",
                    'GEN_COMMENT': "Test",
                    'STF_FEATURES': []
                },
                'trace_interval': interval
            }
        }

        return metadata
        
    def get_workload_sha256(self, workload_path):
        hash_sha256 = hashlib.sha256()
        with open(workload_path, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_sha256.update(chunk)
        return hash_sha256.hexdigest()
    
    def delete_test_traces(self):
        self._delete_folder_and_files(self.test_storage_path)

    def delete_test_storage(self, type, path):
        if type == "local-storage":
            self._delete_folder_and_files(path)

    def _delete_folder_and_files(self, path):
        if not os.path.exists(path):
            return

        for root, dirs, files in os.walk(path, topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))
            for name in dirs:
                os.rmdir(os.path.join(root, name))
                
        os.rmdir(path)
