#!/usr/bin/env python3
"""Build RISC-V workloads using board configuration (schema v2)."""
from __future__ import annotations

import argparse
import glob
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Optional
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from flow.utils.config import BuildConfig, FeatureSet, FinalConfig
from flow.utils.paths import BenchmarkPaths, binaries_root
from flow.utils.util import CommandError, Util


# -----------------------------------------------------------------------------
# Helper utilities
# -----------------------------------------------------------------------------


def _unique(paths: Iterable[Path]) -> List[Path]:
    seen = set()
    out: List[Path] = []
    for path in paths:
        key = path.resolve()
        if key not in seen:
            out.append(path)
            seen.add(key)
    return out


def _collect_patterns(patterns: Iterable[str]) -> List[Path]:
    matches: List[Path] = []
    for pattern in patterns:
        for match in glob.glob(pattern, recursive=True):
            candidate = Path(match)
            if candidate.is_file():
                matches.append(candidate)
    return _unique(sorted(matches))


def _extract_isa(cflags: Iterable[str], default: Optional[str]) -> Optional[str]:
    for flag in cflags:
        if flag.startswith("-march="):
            return flag.split("=", 1)[1]
    return default


def _emit_build_metadata(paths: BenchmarkPaths, final: FinalConfig, *, bench: str, args: argparse.Namespace, sources: List[str]) -> None:
    metadata = {
        "timestamp": Util.now_iso(),
        "emulator": args.emulator,
        "arch": args.arch,
        "platform": args.platform,
        "workload": args.workload,
        "benchmark": bench,
        "features": {"bbv": args.bbv, "trace": args.trace},
        "entrypoint": args.entrypoint or "benchmark",
        "toolchain": {"cc": final.tools.cc},
        "flags": {
            "cflags": final.flags.cflags,
            "includes": final.flags.includes,
            "ldflags": final.flags.ldflags,
            "libs": final.flags.libs,
            "linker_script": str(final.flags.linker_script) if final.flags.linker_script else None,
        },
        "env": {
            "skip": final.env.skip,
            "dir": str(final.env.dir),
            "files": final.env.files,
        },
        "isa": _extract_isa(final.flags.cflags, None),
        "sources": sources,
        "binary": str(paths.binary_path),
    }
    Util.write_json(paths.build_meta_path, metadata)


@dataclass
class CompilationArtifacts:
    env_objs: List[Path]
    common_objs: List[Path]
    support_once_objs: List[Path]


class WorkloadBuilder:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.config = BuildConfig.load(board=args.emulator)
        self.features = FeatureSet(bbv=args.bbv, trace=args.trace)
        if not args.list and not args.workload and not args.input_obj:
            raise SystemExit("--workload is required unless --input-obj is provided")

    # ------------------------------------------------------------------
    # Public entrypoints
    # ------------------------------------------------------------------
    def list_workloads(self) -> None:
        Util.info("Configured workloads:")
        for workload in self.config.list_workloads():
            try:
                root = self.config.resolve_workload_root(workload, None)
                final = self._final_config(workload, root)
            except SystemExit:
                Util.warn(f"  {workload}: failed to resolve")
                continue

            if final.layout.mode == "per_benchmark" and final.layout.per_benchmark:
                benches = self._discover_benchmarks(final)
                sample = ", ".join(benches[:6]) + (" …" if len(benches) > 6 else "")
                Util.info(f"  {workload} ({final.layout.mode}): {root} | benches: {sample}")
            else:
                Util.info(f"  {workload} ({final.layout.mode}): {root}")

    def build(self) -> None:
        if not self.args.workload:
            raise SystemExit("--workload is required to select configuration data")
        workload_root = self.config.resolve_workload_root(self.args.workload, self.args.custom_path)
        final = self._final_config(self.args.workload, workload_root)

        # Always ensure env dir exists even if skipped, so downstream tooling has predictable paths.
        env_dir = binaries_root(self.args.emulator) / "env"
        env_dir.mkdir(parents=True, exist_ok=True)

        if self.args.input_obj:
            self._link_from_objects(final)
            return

        if final.layout.mode == "per_benchmark" and final.layout.per_benchmark:
            bench_names = self._target_benchmarks(final)
            artifacts = self._prepare_shared_artifacts(final)
            for bench in bench_names:
                self._build_benchmark(final, artifacts, bench)
        else:
            self._build_single(final)

    # ------------------------------------------------------------------
    # Build helpers
    # ------------------------------------------------------------------
    def _final_config(self, workload: str, workload_root: Path) -> FinalConfig:
        return self.config.finalize(
            workload=workload,
            arch=self.args.arch,
            platform=self.args.platform,
            emulator=self.args.emulator,
            workload_root=workload_root,
            features=self.features,
        )

    def _prepare_shared_artifacts(self, final: FinalConfig) -> CompilationArtifacts:
        env_objs = self._compile_environment(final)
        common_objs = self._compile_common_sources(final)
        support_once_objs = self._compile_support_once_sources(final)
        return CompilationArtifacts(env_objs, common_objs, support_once_objs)

    def _compile_environment(self, final: FinalConfig) -> List[Path]:
        if final.env.skip:
            return []
        objects: List[Path] = []
        env_out = binaries_root(self.args.emulator) / "env"
        env_out.mkdir(parents=True, exist_ok=True)
        for file_name in final.env.files:
            src = final.env.dir / file_name
            if not src.exists():
                raise SystemExit(f"Environment file missing: {src}")
            obj = env_out / (Path(file_name).stem + ".o")
            self._compile(src, obj, final)
            objects.append(obj)
        return objects

    def _compile_common_sources(self, final: FinalConfig) -> List[Path]:
        patterns = final.layout.common_patterns
        if not patterns:
            return []
        sources = _collect_patterns(patterns)
        skip = {Path(item).stem for item in final.layout.common_skip}
        sources = [s for s in sources if s.stem not in skip]
        dest = binaries_root(self.args.emulator) / self.args.workload / "common" / "obj"
        return self._compile_many(sources, final, dest)

    def _compile_support_once_sources(self, final: FinalConfig) -> List[Path]:
        patterns = final.layout.support_once_patterns
        if not patterns:
            return []
        sources = _collect_patterns(patterns)
        dest = binaries_root(self.args.emulator) / self.args.workload / "support_once" / "obj"
        return self._compile_many(sources, final, dest)

    def _target_benchmarks(self, final: FinalConfig) -> List[str]:
        if self.args.benchmark:
            return [self.args.benchmark]
        return self._discover_benchmarks(final)

    def _discover_benchmarks(self, final: FinalConfig) -> List[str]:
        per = final.layout.per_benchmark
        if not per:
            return []
        root = Path(per.bench_root)
        if not root.exists():
            raise SystemExit(f"Bench root missing: {root}")
        names = [entry.name for entry in sorted(root.iterdir()) if entry.is_dir()]
        exclude = set(per.exclude_dirs)
        return [name for name in names if name not in exclude]

    def _collect_benchmark_sources(self, final: FinalConfig, bench: str) -> List[Path]:
        per = final.layout.per_benchmark
        if not per:
            return []
        root = Path(per.bench_root) / bench
        patterns = [str(root / pattern) for pattern in per.source_patterns]
        bench_sources = _collect_patterns(patterns)

        # Support files scoped per benchmark
        support_patterns = [pattern.replace("{bench}", bench) for pattern in final.layout.support_per_benchmark_patterns]
        bench_sources += _collect_patterns(support_patterns)
        return _unique(bench_sources)

    def _compile_many(self, sources: Iterable[Path], final: FinalConfig, dest_dir: Path) -> List[Path]:
        dest_dir.mkdir(parents=True, exist_ok=True)
        objects: List[Path] = []
        for src in sources:
            obj = dest_dir / (src.stem + ".o")
            self._compile(src, obj, final)
            objects.append(obj)
        return objects

    def _compile(self, source: Path, output: Path, final: FinalConfig) -> None:
        output.parent.mkdir(parents=True, exist_ok=True)
        cmd = [final.tools.cc, "-c", *final.flags.cflags_with_includes, "-o", str(output), str(source)]
        try:
            Util.run_cmd(cmd)
        except CommandError as err:
            raise SystemExit(str(err))

    def _link(self, final: FinalConfig, output: Path, objects: List[Path]) -> None:
        cmd = [final.tools.cc]
        if final.flags.linker_script:
            cmd.extend([f"-T{final.flags.linker_script}"])
        cmd.extend(final.flags.ldflags)
        cmd.extend(["-o", str(output)])
        cmd.extend(str(obj) for obj in objects)
        cmd.extend(final.flags.libs)
        try:
            Util.run_cmd(cmd)
        except CommandError as err:
            raise SystemExit(str(err))

    def _maybe_wrapper(self, final: FinalConfig) -> Optional[Path]:
        entry = self.args.entrypoint
        if not entry or entry == "benchmark":
            return None
        wrapper_dir = binaries_root(self.args.emulator) / "wrapper"
        wrapper_dir.mkdir(parents=True, exist_ok=True)
        src = wrapper_dir / "wrapper_entry.c"
        src.write_text(
            f"extern int {entry}(void);\n"
            "int benchmark(void) {\n"
            f"    return {entry}();\n"
            "}\n"
        )
        obj = wrapper_dir / "wrapper_entry.o"
        self._compile(src, obj, final)
        return obj

    def _build_benchmark(self, final: FinalConfig, artifacts: CompilationArtifacts, bench: str) -> None:
        paths = BenchmarkPaths(self.args.emulator, self.args.workload, bench)
        paths.resolve()

        bench_sources = self._collect_benchmark_sources(final, bench)
        if not bench_sources:
            raise SystemExit(f"No sources discovered for benchmark '{bench}'")

        objects = self._compile_many(bench_sources, final, paths.object_dir)
        wrapper_obj = self._maybe_wrapper(final)
        link_inputs = artifacts.env_objs + artifacts.common_objs + artifacts.support_once_objs + objects
        if wrapper_obj:
            link_inputs.append(wrapper_obj)

        self._link(final, paths.binary_path, link_inputs)
        Util.info(f"Built {paths.binary_path}")
        _emit_build_metadata(
            paths,
            final,
            bench=bench,
            args=self.args,
            sources=[str(src) for src in bench_sources],
        )

    def _build_single(self, final: FinalConfig) -> None:
        patterns = final.layout.single_sources
        if not patterns:
            raise SystemExit("Single workload requested but no sources defined")
        sources = _collect_patterns(patterns)
        if not sources:
            raise SystemExit("Could not resolve any sources for single workload")

        bench_name = self.args.benchmark or self.args.workload
        paths = BenchmarkPaths(self.args.emulator, self.args.workload, bench_name)
        paths.resolve()

        env_objs = self._compile_environment(final)
        support_once_objs = self._compile_support_once_sources(final)
        objects = self._compile_many(sources, final, paths.object_dir)
        wrapper_obj = self._maybe_wrapper(final)

        link_inputs = env_objs + support_once_objs + objects
        if wrapper_obj:
            link_inputs.append(wrapper_obj)

        self._link(final, paths.binary_path, link_inputs)
        Util.info(f"Built {paths.binary_path}")
        _emit_build_metadata(
            paths,
            final,
            bench=bench_name,
            args=self.args,
            sources=[str(src) for src in sources],
        )

    def _link_from_objects(self, final: FinalConfig) -> None:
        if not self.args.benchmark:
            raise SystemExit("--benchmark must be provided when using --input-obj")
        paths = BenchmarkPaths(self.args.emulator, self.args.workload, self.args.benchmark)
        paths.resolve()

        env_objs = self._compile_environment(final)
        wrapper_obj = self._maybe_wrapper(final)

        link_inputs = env_objs + [Path(obj) for obj in self.args.input_obj]
        if wrapper_obj:
            link_inputs.append(wrapper_obj)

        self._link(final, paths.binary_path, link_inputs)
        Util.info(f"Linked {paths.binary_path} from provided objects")
        _emit_build_metadata(
            paths,
            final,
            bench=self.args.benchmark,
            args=self.args,
            sources=[str(Path(obj).resolve()) for obj in self.args.input_obj],
        )


# -----------------------------------------------------------------------------
# CLI
# -----------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build RISC-V workloads")
    parser.add_argument("--workload", required=False, help="Workload name as defined in board.yaml")
    parser.add_argument("--benchmark", help="Specific benchmark to build (per_benchmark mode)")
    parser.add_argument("--arch", default="rv32", choices=["rv32", "rv64"])
    parser.add_argument("--platform", default="baremetal", choices=["baremetal", "linux"])
    parser.add_argument("--emulator", default="spike", choices=["spike", "qemu"])
    parser.add_argument("--bbv", action="store_true", help="Enable BBV feature macros during compilation")
    parser.add_argument("--trace", action="store_true", help="Enable trace feature macros during compilation")
    parser.add_argument("--custom-path", help="Override workload source root")
    parser.add_argument("--entrypoint", help="Create benchmark() wrapper that calls the provided function")
    parser.add_argument("--input-obj", nargs="+", help="Link provided object/archive files instead of building sources")
    parser.add_argument("--list", action="store_true", help="List workloads from board.yaml and exit")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    builder = WorkloadBuilder(args)
    if args.list:
        builder.list_workloads()
        return
    if not args.workload and not args.input_obj:
        raise SystemExit("--workload is required unless --input-obj is provided")
    builder.build()


if __name__ == "__main__":
    main()
