#!/usr/bin/env python3
"""Launch helper for running commands inside the project Docker image."""
from __future__ import annotations

import shlex
import subprocess
from pathlib import Path
from typing import Iterable, Sequence

from data.consts import Const
from flow.utils.util import CommandError, CommandResult, Util


class DockerOrchestrator:
    """Small wrapper around ``docker run`` with the mounts we expect."""

    def __init__(self, image: str | None = None) -> None:
        self.image = image or Const.DOCKER_IMAGE_NAME
        self.project_root = Path(__file__).resolve().parents[2]
        self.mounts = self._default_mounts()

    def _default_mounts(self) -> list[tuple[Path, str]]:
        mounts: list[tuple[Path, str]] = []
        mounts.append((self.project_root, Const.CONTAINER_FLOW_ROOT))
        outputs = self.project_root / "outputs"
        outputs.mkdir(parents=True, exist_ok=True)
        mounts.append((outputs, Const.CONTAINER_OUTPUT_ROOT))
        env_dir = self.project_root / "environment"
        if env_dir.exists():
            mounts.append((env_dir, Const.CONTAINER_ENV_ROOT))
        workloads_dir = self.project_root.parent / "workloads"
        if workloads_dir.exists():
            mounts.append((workloads_dir, Const.CONTAINER_WORKLOAD_ROOT))
        return mounts

    def _docker_prefix(self, workdir: str, interactive: bool) -> list[str]:
        cmd = ["docker", "run", "--rm"]
        if interactive:
            cmd.append("-it")
        for host, container in self.mounts:
            cmd.extend(["-v", f"{host.resolve()}:{container}"])
        cmd.extend(["-w", workdir, self.image])
        return cmd

    def run(
        self,
        command: Sequence[str],
        *,
        workdir: str = Const.CONTAINER_FLOW_ROOT,
        interactive: bool = False,
    ) -> subprocess.CompletedProcess:
        docker_cmd = self._docker_prefix(workdir, interactive) + ["bash", "-lc", shlex.join(command)]
        Util.info("Docker exec: " + " ".join(docker_cmd))
        result = subprocess.run(docker_cmd)
        if result.returncode != 0:
            raise CommandError(CommandResult(tuple(docker_cmd), result.returncode, "", ""))
        return result
