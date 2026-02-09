"""
config_v2.py — strict YAML parser and config finalizer for the builder.

Schema overview (see board.yaml for a concrete example):

schema: 2
variables:
  workloads_roots: ["/workloads", "/default"]
  env_root: "/default/environment/{board}"
  outputs_root: "/outputs/{emulator}/bin"
  include_auto: []          # optional extra includes to add everywhere
  alt_roots:                # optional fallbacks if a path doesn't exist
    - {from: "/workloads/", to: "/default/"}

toolchains:
  rv32:
    baremetal:
      cc: "riscv32-unknown-elf-gcc"
      base_cflags: ["-march=rv32imafdc", ...]
      base_ldflags: ["-march=rv32imafdc", ...]
      libs: ["-lc", "-lm"]
      linker_script: "link.ld"
    linux:
      cc: "riscv32-unknown-linux-gnu-gcc"
      base_cflags: [...]
      base_ldflags: [...]
      libs: ["-lm"]

features:
  bbv:
    cflags: ["-DBBV"]
    ldflags: []
  trace:
    cflags: ["-DTRACE"]
    ldflags: []
  # (Optional) arch_overrides for vector vs regular can also be modeled here.

workloads:
  <name>:
    # Layout + flags which can be overridden under platforms.<platform>
    layout:
      mode: "per_benchmark" | "single"
      # per_benchmark only:
      per_benchmark:
        bench_root: "{workload_root}/benchmarks"  # or .../src
        source_patterns: ["*.c", "*.S"]
        exclude_dirs: ["common"]                  # optional
      common_patterns: ["{workload_root}/benchmarks/common/*.c"]   # optional
      common_skip: ["syscalls.c"]                                   # optional
      support_once_patterns: ["{workload_root}/support/*.c"]        # optional
      support_per_benchmark_patterns: ["{workload_root}/support/*.c"]  # optional
      # single only:
      single_sources: ["{workload_root}/foo.c", ...]                # if mode: single

    includes: ["{workload_root}/env"]            # as many as you need
    defines: []
    cflags: []
    ldflags: []
    libs: []

    env:
      files: ["crt0.S", "main.c", "stub.c", "util.c"]
      skip: false

    platforms:
      baremetal: { ... overrides ... }
      linux:     { ... overrides ... }
"""

from __future__ import annotations
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional
import yaml
import copy

from data.consts import Const

# ---------------------------- Data Classes ----------------------------

@dataclass
class Toolchain:
    cc: str
    base_cflags: List[str]
    base_ldflags: List[str]
    libs: List[str]
    linker_script: Optional[str]


@dataclass
class Flags:
    cflags: List[str]
    includes: List[str]
    ldflags: List[str]
    libs: List[str]
    linker_script: Optional[Path]

    @property
    def cflags_with_includes(self) -> List[str]:
        incs = [f"-I{p}" for p in self.includes]
        # dedupe preserving order
        seen = set()
        out: List[str] = []
        for f in (self.cflags + incs):
            if f not in seen:
                out.append(f)
                seen.add(f)
        return out


@dataclass
class EnvConfig:
    dir: Path
    files: List[str]
    skip: bool


@dataclass
class PerBenchmark:
    bench_root: str
    source_patterns: List[str]
    exclude_dirs: List[str]


@dataclass
class LayoutConfig:
    mode: str  # "per_benchmark" or "single"
    per_benchmark: Optional[PerBenchmark]
    common_patterns: List[str]
    common_skip: List[str]
    support_once_patterns: List[str]
    support_per_benchmark_patterns: List[str]
    single_sources: List[str]


@dataclass
class Paths:
    workloads_roots: List[Path]
    outputs_root: Path


@dataclass
class Tools:
    cc: str


@dataclass
class FeatureSet:
    bbv: bool = False
    trace: bool = False


@dataclass
class FinalConfig:
    tools: Tools
    flags: Flags
    env: EnvConfig
    layout: LayoutConfig
    paths: Paths


# ---------------------------- Helpers ----------------------------

def _deep_merge(a: dict, b: dict) -> dict:
    """Deep-merge b into a without modifying inputs."""
    out = copy.deepcopy(a)
    for k, v in b.items():
        if isinstance(v, dict) and isinstance(out.get(k), dict):
            out[k] = _deep_merge(out[k], v)
        else:
            out[k] = copy.deepcopy(v)
    return out


def _dedupe(seq: List[str]) -> List[str]:
    seen = set()
    out: List[str] = []
    for x in seq:
        if x not in seen:
            out.append(x)
            seen.add(x)
    return out


def _fmt(value: Any, ctx: Dict[str, str]) -> Any:
    if isinstance(value, str):
        try:
            return value.format(**ctx)
        except KeyError:
            return value
    if isinstance(value, list):
        return [_fmt(v, ctx) for v in value]
    if isinstance(value, dict):
        return {k: _fmt(v, ctx) for k, v in value.items()}
    return value


def _ensure_list(v: Any) -> List:
    if v is None:
        return []
    return v if isinstance(v, list) else [v]


def _rewrite_if_missing(p: Path, alt_rules: List[dict]) -> Path:
    """If p doesn't exist, try replacing configured prefixes."""
    if p.exists():
        return p
    s = str(p)
    for rule in alt_rules or []:
        frm = rule.get("from")
        to = rule.get("to")
        if frm and to and s.startswith(frm):
            candidate = Path(to + s[len(frm):])
            if candidate.exists():
                return candidate
    return p  # keep original (may be created later or be optional)


# ---------------------------- Main Loader ----------------------------

class BuildConfig:
    def __init__(self, data: Dict[str, Any], board: str):
        self.raw = data
        self.board = board

        if int(self.raw.get("schema", 0)) != 2:
            raise SystemExit("board.yaml must set schema: 2")

        self.variables = self.raw.get("variables", {})
        self.toolchains = self.raw.get("toolchains", {})
        self.features = self.raw.get("features", {})
        self.workloads = self.raw.get("workloads", {})

    @staticmethod
    def load(board: str) -> "BuildConfig":
        candidate_paths = [
            Path(Const.CONTAINER_ENV_ROOT) / board / "board.yaml",
            Path(__file__).resolve().parents[2] / "environment" / board / "board.yaml",
        ]
        cfg_path = next((p for p in candidate_paths if p.exists()), None)
        if cfg_path is None:
            raise SystemExit(f"Board config not found for {board}. Searched: {', '.join(str(p) for p in candidate_paths)}")
        with open(cfg_path, "r") as f:
            data = yaml.safe_load(f) or {}
        return BuildConfig(data, board=board)

    def list_workloads(self) -> List[str]:
        return sorted(self.workloads.keys())

    # ---------- Resolution ----------

    def resolve_workload_root(self, workload: str, custom: Optional[str]) -> Path:
        ctx = self._ctx_defaults()
        roots = [Path(_fmt(p, ctx)) for p in self.variables.get("workloads_roots", ["/workloads", "/default"])]
        if custom:
            p = Path(custom)
            if p.exists():
                return p
            raise SystemExit(f"Custom workload path not found: {p}")
        for r in roots:
            candidate = r / workload
            if candidate.exists():
                return candidate
        raise SystemExit(f"Workload '{workload}' not found in: {', '.join(map(str, roots))}")

    def _ctx_defaults(self, **extra) -> Dict[str, str]:
        ctx = dict(
            board=self.board,
            emulator=self.board,  # alias
        )
        ctx.update(extra)
        return ctx

    def _toolchain(self, arch: str, platform: str) -> Toolchain:
        t = self.toolchains.get(arch, {}).get(platform, {})
        if not t:
            raise SystemExit(f"Missing toolchain for {arch}.{platform}")
        return Toolchain(
            cc=t.get("cc", ""),
            base_cflags=_ensure_list(t.get("base_cflags")),
            base_ldflags=_ensure_list(t.get("base_ldflags")),
            libs=_ensure_list(t.get("libs")),
            linker_script=t.get("linker_script"),
        )

    def _apply_features(self, cflags: List[str], ldflags: List[str], feats: FeatureSet) -> Tuple[List[str], List[str]]:
        def add(kind: str):
            cfg = self.features.get(kind, {})
            return _ensure_list(cfg.get("cflags")), _ensure_list(cfg.get("ldflags"))
        cc, ll = [], []
        if feats.bbv:
            c, l = add("bbv")
            cc += c; ll += l
        if feats.trace:
            c, l = add("trace")
            cc += c; ll += l
        return _dedupe(cflags + cc), _dedupe(ldflags + ll)

    def _final_includes(self, base: List[str], ctx: Dict[str, str]) -> List[str]:
        # includes from variables.include_auto + workload/includes
        auto = _ensure_list(self.variables.get("include_auto", []))
        incs = _fmt(auto + base, ctx)
        # rewrite missing prefixes if requested
        alt_rules = _ensure_list(self.variables.get("alt_roots", []))
        fixed: List[str] = []
        for inc in incs:
            p = Path(inc)
            p2 = _rewrite_if_missing(p, alt_rules)
            fixed.append(str(p2))
        return _dedupe(fixed)

    def get_layout(self, workload: str, emulator: str, arch: str, platform: str, workload_root: Path) -> LayoutConfig:
        # Merge workload + platform override, then format
        base = self.workloads.get(workload, {})
        plat = (base.get("platforms", {}) or {}).get(platform, {})
        merged = _deep_merge(base, plat)

        ctx = self._ctx_defaults(workload_root=str(workload_root), arch=arch, platform=platform, emulator=emulator)
        merged = _fmt(merged, ctx)

        layout = merged.get("layout", {}) or {}
        mode = layout.get("mode", "per_benchmark")

        per_bench_config = None
        if mode == "per_benchmark":
            pb = layout.get("per_benchmark", {}) or {}
            per_bench_config = PerBenchmark(
                bench_root=pb.get("bench_root", f"{workload_root}/benchmarks"),
                source_patterns=_ensure_list(pb.get("source_patterns", ["*.c", "*.S"])),
                exclude_dirs=_ensure_list(pb.get("exclude_dirs", [])),
            )

        common_patterns = _ensure_list(layout.get("common_patterns", []))
        common_skip = _ensure_list(layout.get("common_skip", []))
        support_once = _ensure_list(layout.get("support_once_patterns", []))
        support_per_bench = _ensure_list(layout.get("support_per_benchmark_patterns", []))
        single_sources = _ensure_list(layout.get("single_sources", []))

        return LayoutConfig(
            mode=mode,
            per_benchmark=per_bench_config,
            common_patterns=common_patterns,
            common_skip=common_skip,
            support_once_patterns=support_once,
            support_per_benchmark_patterns=support_per_bench,
            single_sources=single_sources,
        )

    def finalize(
        self,
        workload: str,
        arch: str,
        platform: str,
        emulator: str,
        workload_root: Path,
        features: FeatureSet,
    ) -> FinalConfig:
        # toolchain
        tc = self._toolchain(arch, platform)

        # workload->platform deep merge + format
        base = self.workloads.get(workload, {})
        plat = (base.get("platforms", {}) or {}).get(platform, {})
        merged = _deep_merge(base, plat)

        ctx = self._ctx_defaults(workload_root=str(workload_root), arch=arch, platform=platform, emulator=emulator)
        merged = _fmt(merged, ctx)

        # env
        env_cfg = merged.get("env", {}) or {}
        env_skip = bool(env_cfg.get("skip", False))
        env_dir = Path(_fmt(self.variables.get("env_root", "/default/environment/{board}"), ctx))
        env_files = _ensure_list(env_cfg.get("files", []))

        # flags (base toolchain + workload additions)
        defines = [f"-D{d}" for d in _ensure_list(merged.get("defines", []))]
        cflags = _dedupe(tc.base_cflags + defines + _ensure_list(merged.get("cflags", [])))
        ldflags = _dedupe(tc.base_ldflags + _ensure_list(merged.get("ldflags", [])))
        libs = _dedupe(tc.libs + _ensure_list(merged.get("libs", [])))
        # features
        cflags, ldflags = self._apply_features(cflags, ldflags, features)

        # includes
        includes = self._final_includes(_ensure_list(merged.get("includes", [])), ctx)

        # linker script
        lds = merged.get("linker_script", tc.linker_script)
        linker_script = Path(env_dir / lds) if (lds and platform == "baremetal") else None

        # layout
        layout = self.get_layout(workload, emulator=emulator, arch=arch, platform=platform, workload_root=workload_root)

        # outputs root
        out_root = Path(_fmt(self.variables.get("outputs_root", "/outputs/{emulator}/bin"), ctx))

        # alt_roots apply to linker_script path too (if missing)
        alt_rules = _ensure_list(self.variables.get("alt_roots", []))
        if linker_script:
            linker_script = _rewrite_if_missing(linker_script, alt_rules)

        return FinalConfig(
            tools=Tools(cc=tc.cc),
            flags=Flags(
                cflags=cflags,
                includes=includes,
                ldflags=ldflags,
                libs=libs,
                linker_script=linker_script,
            ),
            env=EnvConfig(dir=env_dir, files=env_files, skip=env_skip),
            layout=layout,
            paths=Paths(
                workloads_roots=[Path(p) for p in _fmt(self.variables.get("workloads_roots", ["/workloads", "/default"]), ctx)],
                outputs_root=out_root,
            ),
        )
