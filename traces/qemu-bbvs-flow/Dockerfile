FROM ubuntu:22.04

# Avoid interactive prompts during installation
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

# Update package lists
RUN apt update && apt upgrade -y

# Install all dependencies for QEMU compilation and workloads
RUN apt install -y \
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
  gcc-riscv64-linux-gnu \
  libslirp-dev \
  pkg-config \
  libpixman-1-dev \
  python3-dev \
  python3-venv

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
RUN mkdir -p /workloads /output

# Copy workload setup script
COPY setup_workload.sh /setup_workload.sh
RUN chmod +x /setup_workload.sh

# Copy analysis script
COPY run_analysis.sh /run_analysis.sh
RUN chmod +x /run_analysis.sh

WORKDIR /
