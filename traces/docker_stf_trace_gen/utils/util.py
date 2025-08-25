import sys
import subprocess
import time
from pathlib import Path
from typing import List, Tuple, Optional
import shutil
import logging
from enum import Enum

class LogLevel(Enum):
    INFO = "\033[32m"
    ERROR = "\033[31m"
    WARN = "\033[33m"
    DEBUG = "\033[34m"
    HEADER = "\033[95m\033[1m"

logging.basicConfig(level=logging.INFO, format="[%(asctime)s] %(levelname)s: %(message)s", datefmt="%H:%M:%S")

def log(level: LogLevel, msg: str, file=sys.stdout):
    """Log with ANSI color and timestamp; raises on ERROR."""
    color = level.value
    print(f"{color}{msg}\033[0m", file=file if level != LogLevel.ERROR else sys.stderr)
    if level == LogLevel.ERROR:
        exit(1)
        raise RuntimeError(msg) # do we want to raise here? 

    

def run_cmd(cmd: List[str], cwd: Optional[Path] = None, timeout: int = 300, show: bool = True) -> Tuple[bool, str, str]:
    """Run command, return (success, stdout, stderr)."""
    if show:
        log(LogLevel.DEBUG, f"Running: {' '.join(map(str, cmd))}")
    try:
        result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout, check=False)
        if not result.returncode == 0:
            log(LogLevel.ERROR, f"Command failed: {result.stderr}")
        return result.returncode == 0, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        log(LogLevel.ERROR, f"Timeout after {timeout}s")
    except Exception as e:
        log(LogLevel.ERROR, f"Exception: {e}")

def get_time() -> float:
    """Return current time in seconds."""
    return time.time()

def ensure_dir(path: Path) -> Path:
    """Create directory if it doesn't exist."""
    path.mkdir(parents=True, exist_ok=True)
    return path

def clean_dir(path: Path) -> Path:
    """Clean and recreate directory."""
    if path.exists():
        shutil.rmtree(path)
    return ensure_dir(path)

def validate_tool(tool: str):
    """Check if tool is in PATH."""
    if not shutil.which(tool):
        log(LogLevel.ERROR, f"Tool not found: {tool}")
        return False
    return True

def file_exists(path: Path | str) -> bool:
    """Check if file exists."""
    return Path(path).exists()

def read_file_lines(path: Path) -> List[str]:
    """Read non-empty lines from file."""
    if not file_exists(path):
        log(LogLevel.ERROR, f"File not found: {path}")
    return [line.strip() for line in path.read_text().splitlines() if line.strip()]

def write_file_lines(path: Path, lines: List[str]):
    """Write lines to file."""
    path.write_text("\n".join(lines) + "\n")