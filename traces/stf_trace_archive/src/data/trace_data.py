from dataclasses import dataclass
from typing import Optional

@dataclass
class TraceData():
    path: Optional[str]
    id: Optional[str]
    attempt: Optional[str]
    part: Optional[str]
    metadata_path: Optional[str]
    metadata: Optional[str]