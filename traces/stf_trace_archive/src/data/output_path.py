from dataclasses import dataclass
from typing import Optional


@dataclass
class OutputPaths():
    folder_path: str
    filename: Optional[str]
