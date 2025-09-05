import argparse
from dataclasses import asdict
import os
import yaml
from factories.metadata_factory import MetadataFactory
from data.metadata import Metadata
from utils.trace_generator_arg_parser import parse_args
from utils.docker_orchestrator import DockerOrchestrator
from data.consts import Const
from converters.host_to_docker_path import HostToDockerPathConverter


class TraceGenerator():
    def __init__(self, docker: DockerOrchestrator):
        self.docker = docker
        self.metadataFactory = MetadataFactory(docker)

    def run(self, args: argparse.Namespace) -> None:
        args.output = self._get_ouput_path(args)
        docker_paths = self._convert_paths(args)

        self._run_trace(args, docker_paths)
        metadata = self._generate_metadata(args)
        self._save_metadata(args.output, metadata)

        if args.dump:
            dump_path = f"{args.output}.dump"
            self.docker.run_stf_tool("stf_dump", args.output, dump_path)

    def _get_ouput_path(self, args: argparse.Namespace) -> str:
        if not args.output:
            return f"{args.workload}.zstf"

        isDir = not (os.path.splitext(args.ouput)[1])
        if not isDir and not args.output.endswith("zstf"):
            raise ValueError("Invalid output file extension. Expected .zstf or directory.")

        if not isDir:
            return args.output

        workload_filename = os.path.basename(args.workload)
        return os.path.join(args.output, workload_filename)

    def _convert_paths(
        self,
        args: argparse.Namespace,
        path_arguments: list[str] = ['workload', 'output']
    ) -> dict[str, str]:
        docker_paths = {}
        for path_argument in path_arguments:
            arg_value = getattr(args, path_argument)
            if arg_value:
                docker_paths[arg_value] = HostToDockerPathConverter.convert(arg_value)

        return docker_paths

    def _run_trace(self, args: argparse.Namespace, docker_paths: dict[str, str]):
        bash_cmd = ""
        if args.emulator == "spike":
            bash_cmd = self._get_spike_command(args, docker_paths)
        elif args.emulator == "qemu":
            bash_cmd = self._get_qemu_command(args, docker_paths)
        else:
            raise ValueError(f"Invalid emulator ({args.emulator}) provided")

        self.docker.run_command(bash_cmd, docker_paths)

    def _get_spike_command(self, args: argparse.Namespace, docker_paths: dict[str, str]) -> str:
        isa = f"--isa={args.isa}" if args.isa else ""
        pk = f"{Const.SPKIE_PK}" if args.pk else ""

        if args.mode == "insn_count":
            return f"spike {isa} --stf_trace {docker_paths[args.output]} --stf_trace_memory_records --stf_insn_num_tracing --stf_insn_start {str(args.start_instruction)} --stf_insn_count {str(args.num_instructions)}  {pk} {docker_paths[args.workload]}"
        elif args.mode == "macro":
            return f"spike {isa} --stf_trace {docker_paths[args.output]} --stf_trace_memory_records --stf_macro_tracing {pk} {docker_paths[args.workload]}"

        raise NotImplementedError(f"mode {args.mode} invalid for spike")

    def _get_qemu_command(self, args: argparse.Namespace, docker_paths: dict[str, str]) -> str:
        args.start_instruction += 1
        if args.mode == "insn_count":
            return f"qemu-riscv64 -plugin {Const.LIBSTFMEM},mode=dyn_insn_count,start_dyn_insn={args.start_instruction},num_instructions={args.num_instructions},outfile={docker_paths[args.output]} -d plugin -- {docker_paths[args.workload]}"
        elif args.mode == "pc_count":
            return f"qemu-riscv64 -plugin {Const.LIBSTFMEM},mode=ip,start_ip={args.start_pc},ip_hit_threshold={args.pc_threshold},num_instructions={args.num_instructions},outfile={docker_paths[args.output]} -d plugin -- {docker_paths[args.workload]}"

        raise NotImplementedError(f"mode {args.mode} invalid for qemu")

    def _generate_metadata(self, args: argparse.Namespace) -> Metadata:
        workload_path = args.workload
        return self.metadataFactory.create(
            workload_path=workload_path,
            trace_path=args.output,
            trace_interval_mode=args.mode,
            start_instruction=getattr(args, "start_instruction", None),
            num_instructions=getattr(args, "num_instructions", None),
            start_pc=getattr(args, "start_pc", None),
            pc_threshold=getattr(args, "pc_threshold", None),
            execution_command=None,
            description=None,
        )

    def _save_metadata(self, trace_path: str, metadata: Metadata):
        metadata_path = f"{trace_path}.metadata.yaml"
        with open(metadata_path, 'w') as file:
            yaml.dump(asdict(metadata), file)


def main():
    args = parse_args()
    docker = DockerOrchestrator(args)
    TraceGenerator(docker).run(args)


if __name__ == "__main__":
    main()
