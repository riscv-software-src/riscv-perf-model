
import argparse
import os
import docker
from typing import Optional
from data.consts import Const


class DockerOrchestrator():
    def __init__(self, args: argparse.Namespace) -> None:
        self.docker_image_name = args.image_name if args.image_name else Const.DOCKER_IMAGE_NAME
        self.docker_client = docker.from_env()

    def run_command(self, command: str, binds: Optional[dict[str, str]]) -> str:
        volumes = {}
        for host_path, docker_path in binds.items():
            host_folder = os.path.dirname(host_path)
            docker_folder = os.path.dirname(docker_path)
            volumes[host_folder] = {"bind": docker_folder, "mode": "rw"}

        print(command)
        container = None
        try:
            container = self.docker_client.containers.run(
                image=self.docker_image_name,
                command=["bash", "-c", command],
                volumes=volumes,
                detach=True,
                stdout=True,
                stderr=True
            )

            exit_code = container.wait()["StatusCode"]
            stdout_logs = container.logs(stdout=True, stderr=False)
            stderr_logs = container.logs(stdout=False, stderr=True)

            if exit_code != 0:
                print("Exit code:", exit_code)
                print("STDOUT:\n", stdout_logs.decode())
                print("STDERR:\n", stderr_logs.decode())

        finally:
            try:
                container.remove(force=True)
            except Exception as e:
                print("Cleanup failed:", e)
            return stdout_logs

    def run_stf_tool(self, tool: str, host_input: str, host_output: Optional[str] = None):
        docker_input = self.convert_host_path_to_docker_path(host_input)
        binds = {
            host_input: docker_input,
        }

        tool_path = os.path.join(Const.STF_TOOLS, tool, tool)
        cmd = f"{tool_path} {docker_input}"
        result = self.run_command(cmd, binds)
        if host_output is not None:
            with open(host_output, "wb") as f:
                f.write(result)

        return result

    def convert_host_path_to_docker_path(self, path: str) -> str:
        parts = os.path.abspath(path).strip(os.sep).split(os.sep)
        parts.insert(0, Const.DOCKER_TEMP_FOLDER)
        return os.path.join(*parts)
