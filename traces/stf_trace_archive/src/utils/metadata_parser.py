import yaml

from data.metadata import Metadata
from utils.fields_validator import FieldsValidator


class MetadataParser:
    @staticmethod
    def parse_metadata(metadata_file) -> Metadata:
        data = yaml.safe_load(metadata_file)
        MetadataParser.validate_metadata(data)
        return data

    @staticmethod
    def parse_metadata_from_path(metadata_path: str) -> Metadata:
        with open(metadata_path, 'r') as metadata_file:
            return MetadataParser.parse_metadata(metadata_file)

    @staticmethod
    def validate_metadata(metadata: Metadata) -> None:
        if not metadata:
            raise ValueError("Metadata is empty or invalid.")

        required_keys = {'author': ["name", "company", "email"],
                         'workload': ['filename', 'SHA256', 'execution_command', 'elf_sections'],
                         'stf': ['timestamp', 'stf_trace_info']}
        dependent_keys = {
            'stf': {
                'trace_interval': [
                    'instruction_pc',
                    'pc_count',
                    'interval_lenght',
                    'start_instruction_index',
                    'end_instruction_index'
                ]
            }
        }

        FieldsValidator.validate(metadata, required_keys, dependent_keys)

    @staticmethod
    def is_fully_traced(metadata: Metadata) -> bool:
        return not metadata.get('stf', {}).get('trace_interval', None)
