from dataclasses import dataclass


@dataclass(frozen=True)
class Const:
    DOCKER_IMAGE_NAME = "riscv-perf-model:latest"

    CONTAINER_FLOW_ROOT = "/flow"
    CONTAINER_OUTPUT_ROOT = "/outputs"
    CONTAINER_ENV_ROOT = "/default/environment"
    CONTAINER_WORKLOAD_ROOT = "/workloads"

    LIBSTFMEM = "/usr/lib/libstfmem.so"
    QEMU_PLUGIN_FALLBACK = "/qemu/build/contrib/plugins/libstfmem.so"
    STF_TOOLS = "/riscv/stf_tools/release/tools"
    SPIKE_PK = "/riscv/riscv-pk/build/pk"
