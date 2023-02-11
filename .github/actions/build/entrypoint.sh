#!/usr/bin/env bash

set -x

source "/usr/share/miniconda/etc/profile.d/conda.sh"
conda activate riscv_perf_model

echo "Starting Build Entry"
echo "HOME:" $HOME
echo "GITHUB_WORKSPACE:" $GITHUB_WORKSPACE
echo "GITHUB_EVENT_PATH:" $GITHUB_EVENT_PATH
echo "PWD:" `pwd`

#
# Compile Sparta Infra (always build with release)
#   Have other build types point to release
#
echo "Building Sparta Infra"
cd ${GITHUB_WORKSPACE}/map/sparta
mkdir -p release  # Link step expects "release" as dir name
ln -s release fastdebug
ln -s release debug
cd release
CC=clang CXX=clang++ cmake .. -DCMAKE_BUILD_TYPE=$OLYMPIA_BUILD_TYPE -DGEN_DEBUG_INFO=OFF
make -j2
BUILD_SPARTA=$?
if [ ${BUILD_SPARTA} -ne 0 ]; then
    echo "ERROR: build sparta FAILED!!!"
    exit 1
fi
