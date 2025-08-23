FROM ribeirovsilva/riscv-toolchain:latest

# Avoid interactive prompts during installation
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

# Update package lists and install additional dependencies
RUN apt-get update && apt-get install -y \
  git-email \
  libaio-dev \
  libbluetooth-dev \
  libcapstone-dev \
  libbrlapi-dev \
  libbz2-dev \
  libcap-ng-dev \
  libcurl4-gnutls-dev \
  libgtk-3-dev \
  libibverbs-dev \
  libjpeg8-dev \
  libncurses5-dev \
  libnuma-dev \
  librbd-dev \
  librdmacm-dev \
  libsasl2-dev \
  libsdl2-dev \
  libseccomp-dev \
  libsnappy-dev \
  libssh-dev \
  libvde-dev \
  libvdeplug-dev \
  libvte-2.91-dev \
  libxen-dev \
  liblzo2-dev \
  valgrind \
  xfslibs-dev \
  libnfs-dev \
  libiscsi-dev \
  gcc \
  binutils \
  python3-pip \
  python3-sphinx \
  python3-sphinx-rtd-theme \
  ninja-build \
  flex \
  bison \
  git \
  python3-tomli \
  autoconf \
  automake \
  autotools-dev \
  curl \
  python3 \
  libmpc-dev \
  libmpfr-dev \
  libgmp-dev \
  gawk \
  build-essential \
  texinfo \
  gperf \
  libtool \
  patchutils \
  bc \
  zlib1g-dev \
  libexpat-dev \
  cmake \
  libglib2.0-dev \
  libslirp-dev \
  pkg-config \
  libpixman-1-dev \
  python3-dev \
  python3-venv \
  qemu-user \
  qemu-user-static \
  && rm -rf /var/lib/apt/lists/*

# Clone repositories
RUN git clone https://gitlab.com/qemu-project/qemu.git
RUN git clone https://github.com/hanhwi/SimPoint.git

# Build QEMU with RISC-V support and BBV plugin
WORKDIR /qemu/build
RUN ../configure --target-list=riscv32-linux-user,riscv64-linux-user,riscv32-softmmu,riscv64-softmmu --enable-plugins
RUN make -j$(nproc)
RUN make install

# Set environment variable for QEMU plugins
ENV QEMU_PLUGINS=/qemu/build/contrib/plugins

# Build SimPoint
WORKDIR /SimPoint/
RUN make -j$(nproc)
ENV PATH="$PATH:/SimPoint/bin"

# Create workload directory and output directory
RUN mkdir -p /workloads /output /workspace

# Clone and prepare Embench repository
WORKDIR /workspace
RUN git clone --recursive https://github.com/embench/embench-iot.git
RUN sed -i '36s/^typedef uint8_t bool;/\/\/ &/' /workspace/embench-iot/src/wikisort/libwikisort.c

# Pre-build Embench workloads to avoid build time during analysis
WORKDIR /workspace/embench-iot
RUN python3 ./build_all.py \
    --arch riscv32 \
    --chip generic \
    --board ri5cyverilator \
    --cc riscv64-unknown-elf-gcc \
    --cflags="-c -O2 -ffunction-sections -march=rv32imfdc -mabi=ilp32d" \
    --ldflags="-Wl,-gc-sections -march=rv32imfdc -mabi=ilp32d" \
    --user-libs="-lm"

# Copy setup and analysis scripts
COPY setup_workload.sh /setup_workload.sh
COPY run_analysis.sh /run_analysis.sh
COPY run_embench_simple.sh /run_embench_simple.sh
RUN chmod +x /setup_workload.sh /run_analysis.sh /run_embench_simple.sh

# Verify QEMU works with a test workload
RUN echo "Testing QEMU with pre-built embench workload..." && \
    qemu-riscv32 /workspace/embench-iot/bd/src/crc32/crc32 && \
    echo "QEMU execution test successful"

WORKDIR /