from dataclasses import dataclass
from typing import Optional, Dict


@dataclass
class Author:
    name: Optional[str]
    company: Optional[str]
    email: str


@dataclass
class Workload:
    filename: str
    SHA256: str
    execution_command: str
    elf_sections: Dict[str, str]


@dataclass
class TraceInterval:
    instruction_pc: int
    pc_count: int
    interval_lenght: int
    interval_lenght: int
    start_instruction_index: int
    end_instruction_index: int


@dataclass
class Stf:
    timestamp: str
    stf_trace_info: Dict[str, str]
    trace_interval: Optional[TraceInterval]


@dataclass
class Metadata:
    description: Optional[str]
    author: Author
    workload: Workload
    stf: Stf
