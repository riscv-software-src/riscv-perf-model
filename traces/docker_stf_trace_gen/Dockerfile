FROM ubuntu:24.04

# Set environment variables early
ENV RISCV=/riscv
ENV QEMU_DIR=/qemu
ENV QEMU_PLUGINS=/qemu/build/contrib/plugins
ENV PATH=$RISCV/bin:/opt/riscv/riscv32-elf/bin:/opt/riscv/riscv64-elf/bin:/opt/riscv/riscv32-glibc/bin:/SimPoint/bin:/qemu/build:$PATH 
ENV DEBIAN_FRONTEND=noninteractive
ENV WORKDIR=/workspace
ENV WORKLOADS=/workloads
ENV OUTPUT=/output

# Install dependencies and clean up in one layer
RUN apt update && apt install -y \
    autoconf \
    automake \
    autotools-dev \
    bc \
    binutils \
    bison \
    build-essential \
    cmake \
    curl \
    cython3 \
    device-tree-compiler \
    flex \
    gawk \
    gcc-riscv64-linux-gnu \
    gperf \
    git \
    jq \
    libboost-json-dev \
    libboost-program-options-dev \
    libboost-regex-dev \
    libboost-system-dev \
    libbz2-dev \
    libexpat-dev \
    libglib2.0-dev \
    libgmp-dev \
    libhdf5-dev \
    liblzma-dev \
    libmpc-dev \
    libmpfr-dev \
    libslirp-dev \
    libtool \
    libzstd-dev \
    ninja-build \
    patchutils \
    python3 \
    python3-pip \
    python3-tomli \
    rapidjson-dev \
    texinfo \
    wget \
    zlib1g-dev \
    zstd \
    python3-yaml

# Configure git for building
RUN git config --global url."https://github.com/".insteadOf "git@github.com:" && \
    git config --global user.email "builder@docker.com" && \
    git config --global user.name "Docker Builder"

# Create directory structure
RUN mkdir -p /output

# Clone repositories in RISCV directory
WORKDIR $RISCV
RUN git clone https://github.com/condorcomputing/condor.riscv-isa-sim.git --recurse-submodules
RUN git clone https://github.com/sparcians/stf_tools
RUN git clone https://github.com/riscv-software-src/riscv-pk.git
RUN git clone https://gitlab.com/ribeiro.v.silva/trace-gen.git

# Clone QEMU and SimPoint
WORKDIR /
RUN git clone https://gitlab.com/qemu-project/qemu.git && \
    git clone https://github.com/hanhwi/SimPoint.git

# Clone workloads with Embench PR
WORKDIR /workloads
RUN git clone https://github.com/riscv-software-src/riscv-tests.git && \
    git clone -b spike-as-board https://github.com/widlarizer/embench-iot.git

# Copy and execute toolchain setup script
COPY utils/get-tool.sh $RISCV/get-tool.sh
RUN chmod +x $RISCV/get-tool.sh && \
    $RISCV/get-tool.sh && \
    echo "Toolchain version:" && \
    riscv64-unknown-linux-gnu-gcc --version 2>/dev/null || echo "Toolchain setup pending"

# Build QEMU with plugins support
WORKDIR /qemu/build
RUN ../configure \
    --target-list=riscv32-linux-user,riscv64-linux-user,riscv32-softmmu,riscv64-softmmu \
    --enable-plugins \
    --disable-docs \
    --disable-gtk \
    --disable-sdl 
RUN make -j$(nproc)
RUN make install

# Build SimPoint
WORKDIR /SimPoint
RUN make -j$(nproc)

# Build stf_tools
WORKDIR $RISCV/stf_tools
RUN git submodule update --init --recursive
WORKDIR $RISCV/stf_tools/release
RUN cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)

# Build condor.riscv-isa-sim (Spike)
# make the builddir ? 
WORKDIR $RISCV/condor.riscv-isa-sim/build
RUN ../configure --prefix=$RISCV/condor.riscv-isa-sim/install 
RUN make -j$(nproc) 
RUN make regress 
RUN make install
ENV STF_DIR=$RISCV/condor.riscv-isa-sim/stf_lib

# Create mount points for runtime mounting
# Environment and flow scripts will be mounted at runtime
RUN mkdir -p /workloads/environment /flow /outputs

# Note: The following are mounted at runtime:
# - Host flow scripts -> /flow
# - Host environment -> /workloads/environment  
# - Host outputs -> /outputs

RUN cp $RISCV/condor.riscv-isa-sim/install/bin/spike /usr/bin/

WORKDIR $RISCV/trace-gen
RUN make 
RUN make install

WORKDIR $RISCV/riscv-pk/build
RUN mkdir $RISCV/pk
RUN ../configure --prefix=$RISCV/pk --host=riscv64-unknown-elf
RUN make -j$(nproc)
RUN make install
ENV PATH=$RISCV/pk:$PATH 

WORKDIR /workspace 

CMD ["/bin/bash"]
