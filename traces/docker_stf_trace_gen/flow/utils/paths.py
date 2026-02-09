"""Centralised view of output directories used across the flow."""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from data.consts import Const


def outputs_root() -> Path:
    return Path(Const.CONTAINER_OUTPUT_ROOT)


def binaries_root(emulator: str) -> Path:
    return outputs_root() / emulator / "bin"


def simpoint_analysis_root() -> Path:
    return outputs_root() / "simpoint_analysis"


def simpointed_root(emulator: str) -> Path:
    return outputs_root() / "simpointed" / emulator


@dataclass(frozen=True)
class BenchmarkPaths:
    emulator: str
    workload: str
    benchmark: str

    @property
    def binary_dir(self) -> Path:
        return binaries_root(self.emulator) / self.workload / self.benchmark

    @property
    def binary_path(self) -> Path:
        return self.binary_dir / self.benchmark

    @property
    def object_dir(self) -> Path:
        return self.binary_dir / "obj"

    @property
    def build_meta_path(self) -> Path:
        return self.binary_dir / "build_meta.json"

    @property
    def env_dir(self) -> Path:
        return binaries_root(self.emulator) / "env"

    @property
    def run_root(self) -> Path:
        return outputs_root() / self.emulator / self.workload / self.benchmark

    @property
    def bbv_dir(self) -> Path:
        return self.run_root / "bbv"

    @property
    def trace_dir(self) -> Path:
        return self.run_root / "traces"

    @property
    def logs_dir(self) -> Path:
        return self.run_root / "logs"

    @property
    def run_meta_path(self) -> Path:
        return self.run_root / "run_meta.json"

    @property
    def simpoint_dir(self) -> Path:
        return simpointed_root(self.emulator) / self.workload / self.benchmark

    def resolve(self) -> None:
        """Ensure parent directories exist for consumers that expect them."""
        for path in (self.binary_dir, self.object_dir, self.env_dir, self.run_root):
            path.mkdir(parents=True, exist_ok=True)


__all__ = [
    "BenchmarkPaths",
    "binaries_root",
    "outputs_root",
    "simpoint_analysis_root",
    "simpointed_root",
]
