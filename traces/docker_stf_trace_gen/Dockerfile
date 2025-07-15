FROM ubuntu:24.04

# Combine apt-get commands and clean up to reduce image size
RUN apt-get update && apt-get install -y \
    device-tree-compiler \
    libboost-regex-dev \
    libboost-system-dev \
    libboost-program-options-dev \
    libboost-json-dev \
    rapidjson-dev \
    libhdf5-dev \
    liblzma-dev \
    libbz2-dev \
    cython3 \
    git \
    binutils \
    build-essential \
    cmake \
    gcc-riscv64-linux-gnu \
    zstd \
    libzstd-dev \
    autoconf \
    automake \
    autotools-dev \
    curl \
    python3 \
    python3-pip \
    python3-tomli \
    libmpc-dev \
    libmpfr-dev \
    libgmp-dev \
    gawk \
    bison \
    flex \
    texinfo \
    gperf \
    libtool \
    patchutils \
    bc \
    zlib1g-dev \
    libexpat-dev \
    ninja-build \
    libglib2.0-dev \
    libslirp-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Set up RISCV environment
ENV RISCV=/riscv
ENV PATH=$RISCV/bin:/opt/riscv/bin:$PATH
RUN mkdir -p $RISCV

# Using HTTP instead of SSH to avoid setting git credentials
RUN git config --global url."https://github.com/".insteadOf "git@github.com:"

# Generic config details required during the cloning (git am, apply,patch done during the toolchain build process)
RUN git config --global user.email "builder@example.com" && \
    git config --global user.name "Docker Builder"


# Clone repositories
WORKDIR $RISCV
RUN git clone https://github.com/condorcomputing/condor.riscv-isa-sim.git --recurse-submodules || exit 1
RUN git clone https://github.com/sparcians/stf_tools || exit 1
RUN git clone https://github.com/riscv-software-src/riscv-pk.git || exit 1
RUN git clone https://github.com/riscv-collab/riscv-gnu-toolchain.git || exit 1

# Build riscv-gnu-toolchain
WORKDIR $RISCV/riscv-gnu-toolchain
RUN ./configure --prefix=/opt/riscv && make -j$(nproc) && make install

# Build riscv-pk
WORKDIR $RISCV/riscv-pk/build
RUN ../configure --prefix=$RISCV --host=riscv64-unknown-elf && make -j$(nproc) && make install

# Build stf_tools
WORKDIR $RISCV/stf_tools
RUN git submodule update --init --recursive || exit 1
WORKDIR $RISCV/stf_tools/release
RUN cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)

# Build condor.riscv-isa-sim
WORKDIR $RISCV/condor.riscv-isa-sim
RUN bash scripts/download-bm-compiler.sh && \
    COMPILER_DIR=$(find . -maxdepth 1 -type d -name "riscv-embecosm-*" | head -n 1) && \
    [ -n "$COMPILER_DIR" ] || (echo "Error: riscv-embecosm-* directory not found!" && exit 1) && \
    export PATH=$(pwd)/$COMPILER_DIR/bin:$PATH && \
    echo "export PATH=/riscv/condor.riscv-isa-sim/$COMPILER_DIR/bin:\$PATH" >> /root/.bashrc && \
    mkdir -p build && cd build && \
    ../configure --prefix=$RISCV/condor.riscv-isa-sim/install && \
    make -j$(nproc) && \
    make regress && \
    make install

# shell
CMD ["/bin/bash"]

