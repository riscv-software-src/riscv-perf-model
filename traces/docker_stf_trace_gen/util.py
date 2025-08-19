#!/usr/bin/env python3

import sys
import subprocess
import time
from pathlib import Path

def log(level, msg):
    """Print colored log messages with timestamp"""
    color = {"INFO": "\033[32m", "ERROR": "\033[31m", "DEBUG": "\033[34m"}.get(level, "")
    ts = time.strftime('%H:%M:%S')
    print(f"[{ts}] {color}{level}:\033[0m {msg}", file=sys.stderr if level == "ERROR" else sys.stdout)
    if level == "ERROR": 
        sys.exit(1)

def run_cmd(cmd, error_msg="Command failed", output_file=None):
    """Run a command and exit on failure"""
    print(f"Running: {' '.join(cmd)}")
    try:
        if output_file:
            with open(output_file, 'w') as f:
                result = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, check=True)
        else:
            result = subprocess.run(cmd, check=True, capture_output=True, text=True)
            return result.stdout, result.stderr
    except subprocess.CalledProcessError as e:
        log("ERROR", f"{error_msg}: {e}")

def get_time():
    """Get current time"""
    return time.time()

def ensure_dir(path):
    """Create directory if it doesn't exist"""
    path = Path(path)
    path.mkdir(parents=True, exist_ok=True)
    return path

def clean_dir(path):
    """Clean directory contents"""
    import shutil
    path = Path(path)
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True, exist_ok=True)
    return path

def validate_tool(tool_name):
    """Validate that a tool is available in PATH"""
    if not subprocess.run(["which", tool_name], capture_output=True).returncode == 0:
        log("ERROR", f"Tool not found: {tool_name}")
    return True

def file_exists(path):
    """Check if file exists"""
    return Path(path).exists()

def read_file_lines(path):
    """Read file and return non-empty lines"""
    if not file_exists(path):
        log("ERROR", f"File not found: {path}")
    
    lines = []
    for line in Path(path).read_text().strip().split('\n'):
        if line.strip():
            lines.append(line.strip())
    return lines

def write_file_lines(path, lines):
    """Write lines to file"""
    Path(path).write_text("\n".join(lines) + "\n")