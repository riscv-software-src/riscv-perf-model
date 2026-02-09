#!/bin/bash

# Script to download and install RISC-V 32-bit and 64-bit GNU toolchains
# Supports .tar.gz, .tar.xz, and .zip archives
# Arch: rv32d (ilp32d), rv64d (lp64d), riscv32-glibc

set -euo pipefail

REPO="riscv-collab/riscv-gnu-toolchain"
ASSET_PATTERNS=("riscv32-elf-ubuntu-24.04-gcc-nightly" "riscv64-elf-ubuntu-24.04-gcc-nightly" "riscv32-glibc-ubuntu-24.04-gcc")
INSTALL_DIR="/opt/riscv"
LOG_FILE="/opt/riscv/toolchain_install.log"

mkdir -p "$INSTALL_DIR"

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

extract_archive() {
    local archive="$1"
    local dest="$2"

    case "$archive" in
        *.tar.gz|*.tgz)
            tar -xzf "$archive" -C "$dest" --strip-components=1
            ;;
        *.tar.xz)
            tar -xJf "$archive" -C "$dest" --strip-components=1
            ;;
        *.zip)
            unzip -q "$archive" -d "$dest"
            ;;
        *)
            log "Error: Unknown archive format: $archive"
            return 1
            ;;
    esac
}

for pattern in "${ASSET_PATTERNS[@]}"; do
    log "Fetching latest release for $pattern"
    
    LATEST_ASSET=$(curl -s "https://api.github.com/repos/$REPO/releases" | \
        jq -r --arg pattern "$pattern" \
        '.[] | .assets[] | select(.name | contains($pattern)) | .browser_download_url' | \
        head -n 1)

    if [ -z "$LATEST_ASSET" ]; then
        log "Error: No matching asset found for $pattern"
        exit 1
    fi

    log "Found latest asset: $LATEST_ASSET"

    ARCH=$(echo "$pattern" | grep -o 'riscv[0-9]\+-[a-z]\+')
    EXTRACT_DIR="$INSTALL_DIR/$ARCH"
    TMP_FILE="/tmp/${ARCH}$(basename "$LATEST_ASSET")"

    if [ -f "$TMP_FILE" ]; then
        log "Skipping download, file already exists: $TMP_FILE"
    else
        log "Downloading to $TMP_FILE"
        wget -q --show-progress "$LATEST_ASSET" -O "$TMP_FILE"
        log "Download successful for $ARCH"
    fi

    mkdir -p "$EXTRACT_DIR"
    if extract_archive "$TMP_FILE" "$EXTRACT_DIR"; then
        log "Extraction successful for $ARCH to $EXTRACT_DIR"
    else
        log "Error: Extraction failed for $ARCH"
        exit 1
    fi

    rm -f "$TMP_FILE"
done

log "Toolchain versions:"
cd /opt/riscv/
for arch in riscv32-elf riscv64-elf riscv32-glibc; do
    TOOLCHAIN_BIN="$INSTALL_DIR/$arch/bin/${arch}-gcc"
    if [ -x "$TOOLCHAIN_BIN" ]; then
        VERSION=$("$TOOLCHAIN_BIN" --version | head -n 1)
        log "$arch: $VERSION"
    else
        log "Error: $arch toolchain not found"
        exit 1
    fi
done

log "Toolchain installation completed successfully"
