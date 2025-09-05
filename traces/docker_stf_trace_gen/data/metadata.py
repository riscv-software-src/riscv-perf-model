from dataclasses import dataclass
from typing import Literal, Optional, Dict, Union


@dataclass
class Author:
    email: str
    name: Optional[str] = None
    company: Optional[str] = None


@dataclass
class Workload:
    filename: str
    SHA256: str
    execution_command: Optional[str]
    elf_sections: Dict[str, str]


@dataclass
class IpModeInterval:
    ip: int
    ip_count: int
    interval_lenght: int


@dataclass
class InstructionCountModeInterval:
    start_instruction: int
    interval_lenght: int


@dataclass
class Stf:
    timestamp: str
    stf_trace_info: Dict[str, str]
    interval_mode: Literal["ip", "instructionCount", "macro", "fullyTrace"]
    interval: Optional[Union[IpModeInterval, InstructionCountModeInterval]] = None


@dataclass
class Metadata:
    author: Author
    workload: Workload
    stf: Stf
    description: Optional[str] = None
