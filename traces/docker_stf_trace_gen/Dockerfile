FROM ubuntu:22.04

RUN apt-get update
RUN apt-get install -y device-tree-compiler libboost-regex-dev libboost-system-dev git binutils build-essential cmake gcc-riscv64-linux-gnu zstd libzstd-dev autoconf automake autotools-dev curl python3 python3-pip python3-tomli libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev ninja-build git cmake libglib2.0-dev libslirp-dev

RUN mkdir  riscv
ENV RISCV=/riscv
ENV PATH=$RISCV/bin:$PATH

RUN git clone https://github.com/jeffnye-gh/riscv-isa-sim.git
RUN git clone https://github.com/riscv-software-src/riscv-pk.git
RUN git clone https://github.com/riscv-collab/riscv-gnu-toolchain.git

WORKDIR /riscv-gnu-toolchain
RUN ./configure --prefix=/opt/riscv
ENV PATH=/opt/riscv/bin:$PATH
RUN make -j$(nproc)

WORKDIR /riscv-pk/build
RUN ../configure --prefix=$RISCV --host=riscv64-unknown-elf
RUN make  -j$(nproc)
RUN make install

WORKDIR /riscv-isa-sim/build
RUN git checkout spike_stf
RUN git submodule update --init --recursive
RUN ../configure --prefix=$RISCV
RUN make -j$(nproc)
RUN make install
