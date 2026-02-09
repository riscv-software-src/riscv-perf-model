import datetime
import os
import re
from pathlib import Path
from typing import Dict, Literal, Optional
from elftools.elf.elffile import ELFFile
from utils.util import Util, CommandError
from data.consts import Const
from data.metadata import Author, InstructionCountModeInterval, IpModeInterval, Metadata, Stf, Workload


class MetadataFactory:
    def __init__(self):
        pass

    def create(
        self,
        workload_path: str,
        trace_path: str,
        trace_interval_mode: Literal["ip", "instructionCount", "macro", "fullyTrace"],
        start_instruction: Optional[int] = None,
        num_instructions: Optional[int] = None,
        start_pc: Optional[int] = None,
        pc_threshold: Optional[int] = None,
        execution_command: Optional[str] = None,
        description: Optional[str] = None,
    ) -> Metadata:
        author = Author(name="John Doe", company="Example Corp", email="john.doe@example.com")
        workload = self._create_workload(workload_path, execution_command)
        stf = self._create_stf(
            trace_path,
            trace_interval_mode,
            start_instruction,
            num_instructions,
            start_pc,
            pc_threshold)

        return Metadata(description=description, author=author, workload=workload, stf=stf)

    def _create_workload(self, workload_path: str, execution_command: Optional[str] = None) -> Workload:
        sha256 = Util.compute_sha256(workload_path)
        elf_sections = self._get_workload_sections(workload_path)
        filename = os.path.basename(workload_path)
        return Workload(filename, sha256, execution_command, elf_sections)

    def _create_stf(
        self,
        trace_path: str,
        interval_mode: Literal["ip", "instructionCount", "macro", "fullyTrace"],
        start_instruction: Optional[int] = None,
        num_instructions: Optional[int] = None,
        start_pc: Optional[int] = None,
        pc_threshold: Optional[int] = None,
    ) -> Stf:
        interval = None
        if interval_mode == "ip":
            interval = IpModeInterval(ip=start_pc, ip_count=pc_threshold, interval_lenght=num_instructions)
        elif interval_mode == "instructionCount":
            interval = InstructionCountModeInterval(start_instruction, num_instructions)

        utc_datetime = datetime.datetime.now(datetime.timezone.utc)
        utc_timestamp = utc_datetime.timestamp()

        stf_trace_info = self._get_stf_info(trace_path)
        return Stf(utc_timestamp, stf_trace_info, interval_mode, interval)

    def _get_workload_sections(self, workload_path: str) -> dict[str, str]:
        sections = ['.comment', '.riscv.attributes', '.GCC.command.line']
        result = {}

        if not os.path.exists(workload_path):
            raise FileNotFoundError(f"Workload file not found: {workload_path}")

        with open(workload_path, 'rb') as binary_file:
            elf = ELFFile(binary_file)
            for section in sections:
                section_data = elf.get_section_by_name(section)
                if section_data:
                    strings = re.findall(rb'[\x20-\x7E]{2,}', section_data.data())
                    decoded = ' '.join(s.decode('utf-8') for s in strings)
                    decoded = decoded.replace('\'', '')
                    result[section.lstrip('.')] = decoded

        return result

    def _get_stf_info(self, trace_path: str) -> Dict[str, str]:
        tool = Path(Const.STF_TOOLS) / "stf_trace_info" / "stf_trace_info"
        try:
            result = Util.run_cmd([str(tool), trace_path])
        except CommandError as err:
            Util.error(f"Failed to run stf_trace_info: {err}")
            return {}
        trace_info = result.stdout

        metadata = {}
        values_section = []
        trace_info_lines = trace_info.strip().split('\n')

        in_values = False
        for line in trace_info_lines:
            line = line.strip()
            if len(line.strip()) == 0:
                in_values = True
                continue

            if not in_values:
                if ' ' in line:
                    key, value = line.strip().split(None, 1)
                    metadata[key.strip()] = value.strip()
            else:
                values_section.append(line.strip())

        metadata['STF_FEATURES'] = values_section
        return metadata
