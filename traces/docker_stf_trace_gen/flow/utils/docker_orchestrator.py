#!/usr/bin/env python3
"""Launch helper for running commands inside the project Docker image."""
from __future__ import annotations

import shlex
from pathlib import Path
from typing import Sequence

import docker
from docker.errors import DockerException

from data.consts import Const
from flow.utils.util import CommandError, CommandResult, Util


class DockerOrchestrator:
    """Wrapper around the Python Docker client with our standard mounts."""

    def __init__(self, image: str | None = None) -> None:
        self.image = image or Const.DOCKER_IMAGE_NAME
        self.project_root = Path(__file__).resolve().parents[2]
        self.mounts = self._default_mounts()
        self.client = docker.from_env()

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

    def _volume_bindings(self) -> dict[str, dict[str, str]]:
        volumes: dict[str, dict[str, str]] = {}
        for host, container in self.mounts:
            volumes[str(host.resolve())] = {"bind": container, "mode": "rw"}
        return volumes

    def run(
        self,
        command: Sequence[str],
        *,
        workdir: str = Const.CONTAINER_FLOW_ROOT,
        interactive: bool = False,
    ) -> CommandResult:
        argv = tuple(str(part) for part in command)
        inner_cmd = shlex.join(argv)
        Util.info(f"Docker exec ({workdir}): {inner_cmd}")

        container = None
        stdout = ""
        stderr = ""
        try:
            container = self.client.containers.run(
                image=self.image,
                command=["bash", "-lc", inner_cmd],
                working_dir=workdir,
                volumes=self._volume_bindings(),
                tty=interactive,
                stdin_open=interactive,
                detach=True,
            )
            exit_status = container.wait()
            stdout = (container.logs(stdout=True, stderr=False) or b"").decode()
            stderr = (container.logs(stdout=False, stderr=True) or b"").decode()
            result = CommandResult(argv, exit_status.get("StatusCode", 1), stdout, stderr)
        except DockerException as exc:
            result = CommandResult(argv, 1, stdout, f"Docker error: {exc}")
        finally:
            if container is not None:
                try:
                    container.remove(force=True)
                except DockerException as cleanup_err:
                    Util.warn(f"Docker cleanup failed: {cleanup_err}")

        if not result.ok:
            raise CommandError(result)
        return result
