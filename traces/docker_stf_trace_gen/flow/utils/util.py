"""Common helpers shared across the flow CLIs."""
from __future__ import annotations

import hashlib
import json
import logging
import os
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Mapping, Optional, Sequence, Tuple, Union

# Configure a predictable logger once. Individual scripts can further tweak level.
logging.basicConfig(
    level=logging.INFO,
    format="[%(asctime)s] %(levelname)s: %(message)s",
    datefmt="%H:%M:%S",
)


@dataclass
class CommandResult:
    """Structured command result for downstream processing."""

    argv: Tuple[str, ...]
    returncode: int
    stdout: str
    stderr: str

    @property
    def ok(self) -> bool:
        return self.returncode == 0


class CommandError(RuntimeError):
    """Raised when a subprocess exits with non-zero status and check=True."""

    def __init__(self, result: CommandResult):
        self.result = result
        super().__init__(
            f"Command failed ({result.returncode}): {' '.join(result.argv)}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )


class Util:
    """Grab-bag of helpers for logging, filesystem and subprocess handling."""

    LOGGER = logging.getLogger("flow")

    # ------------------------------------------------------------------
    # Logging helpers
    # ------------------------------------------------------------------
    @staticmethod
    def log(level: Union[int, str], message: str) -> None:
        """Log with python logging while supporting legacy string levels."""
        if isinstance(level, str):
            level = getattr(logging, level.upper(), logging.INFO)
        Util.LOGGER.log(level, message)

    @staticmethod
    def info(message: str) -> None:
        Util.log(logging.INFO, message)

    @staticmethod
    def warn(message: str) -> None:
        Util.log(logging.WARNING, message)

    @staticmethod
    def error(message: str) -> None:
        Util.log(logging.ERROR, message)

    # ------------------------------------------------------------------
    # Subprocess helpers
    # ------------------------------------------------------------------
    @staticmethod
    def run_cmd(
        cmd: Sequence[Union[str, Path]],
        *,
        cwd: Optional[Union[str, Path]] = None,
        env: Optional[Mapping[str, str]] = None,
        timeout: Optional[int] = None,
        check: bool = True,
        capture_output: bool = True,
        text: bool = True,
        log: bool = True,
    ) -> CommandResult:
        """Execute *cmd* and return a :class:`CommandResult`.

        ``check=False`` keeps failures non-fatal so that callers can handle them.
        ``capture_output=False`` streams stdout/stderr directly.
        """
        argv = tuple(str(part) for part in cmd)
        if log:
            Util.LOGGER.debug("Running command: %s", " ".join(argv))
        completed = subprocess.run(
            argv,
            cwd=None if cwd is None else str(cwd),
            env=None if env is None else dict(env),
            timeout=timeout,
            capture_output=capture_output,
            text=text,
            check=False,
        )
        result = CommandResult(
            argv=argv,
            returncode=completed.returncode,
            stdout=completed.stdout or "",
            stderr=completed.stderr or "",
        )
        if check and not result.ok:
            raise CommandError(result)
        return result

    # ------------------------------------------------------------------
    # Filesystem helpers
    # ------------------------------------------------------------------
    @staticmethod
    def ensure_dir(path: Union[str, Path]) -> Path:
        p = Path(path)
        p.mkdir(parents=True, exist_ok=True)
        return p

    @staticmethod
    def clean_dir(path: Union[str, Path]) -> Path:
        p = Path(path)
        if p.exists():
            for child in p.iterdir():
                if child.is_dir():
                    Util.clean_dir(child)
                else:
                    child.unlink()
            p.rmdir()
        p.mkdir(parents=True, exist_ok=True)
        return p

    @staticmethod
    def file_exists(path: Union[str, Path]) -> bool:
        return Path(path).exists()

    @staticmethod
    def glob_paths(
        base: Union[str, Path],
        patterns: Optional[Iterable[str]] = None,
        *,
        include_files: bool = True,
        include_dirs: bool = False,
    ) -> List[Path]:
        base_path = Path(base)
        if not base_path.exists():
            return []
        pats = tuple(patterns or ["*"])
        results: list[Path] = []
        for pattern in pats:
            for entry in base_path.glob(pattern):
                if entry.is_file() and include_files:
                    results.append(entry)
                elif entry.is_dir() and include_dirs:
                    results.append(entry)
        return sorted(set(results))

    @staticmethod
    def read_file_lines(path: Union[str, Path]) -> List[str]:
        p = Path(path)
        if not p.exists():
            raise FileNotFoundError(p)
        return [line.rstrip("\n") for line in p.read_text().splitlines()]

    @staticmethod
    def write_file_lines(path: Union[str, Path], lines: Iterable[str]) -> None:
        Path(path).write_text("\n".join(lines) + "\n")

    @staticmethod
    def read_json(path: Union[str, Path]) -> dict:
        p = Path(path)
        if not p.exists():
            raise FileNotFoundError(p)
        return json.loads(p.read_text())

    @staticmethod
    def write_json(path: Union[str, Path], data: Mapping[str, object]) -> None:
        Path(path).write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")

    # ------------------------------------------------------------------
    # Misc helpers
    # ------------------------------------------------------------------
    @staticmethod
    def compute_sha256(file_path: Union[str, Path]) -> str:
        sha256 = hashlib.sha256()
        with open(file_path, "rb") as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                sha256.update(chunk)
        return sha256.hexdigest()

    @staticmethod
    def validate_tool(tool: str) -> bool:
        if shutil.which(tool):
            return True
        Util.error(f"Required tool not found in PATH: {tool}")
        return False

    @staticmethod
    def now_iso() -> str:
        return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


# Avoid circular import with shutil until after class definition
import shutil  # noqa: E402  (import at end to keep Util definition self-contained)

__all__ = [
    "CommandResult",
    "CommandError",
    "Util",
]
