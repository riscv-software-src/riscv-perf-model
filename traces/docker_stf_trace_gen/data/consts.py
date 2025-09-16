from dataclasses import dataclass


@dataclass(frozen=True)
class Const():
    DOCKER_IMAGE_NAME = "riscv-perf-model:latest"
    DOCKER_TEMP_FOLDER = "/host"
    LIBSTFMEM = "/usr/lib/libstfmem.so"
    STF_TOOLS = "/riscv/stf_tools/release/tools"
    SPKIE_PK = "/riscv/riscv-pk/build/pk"
