from dataclasses import dataclass
from typing import Optional


@dataclass
class TraceData():
    path: Optional[str] = None
    id: Optional[str] = None
    attempt: Optional[str] = None
    part: Optional[str] = None
    metadata_path: Optional[str] = None
    metadata: Optional[str] = None
