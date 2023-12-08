#!/usr/bin/env bash

set -x

source "/usr/share/miniconda/etc/profile.d/conda.sh"
conda activate riscv_perf_model

echo "Starting Build Entry"
echo "HOME:" $HOME
echo "GITHUB_WORKSPACE:" $GITHUB_WORKSPACE
echo "GITHUB_EVENT_PATH:" $GITHUB_EVENT_PATH
echo "CONDA_PREFIX:" $CONDA_PREFIX
echo "PWD:" `pwd`

#
# Compile Sparta Infra (always build with release)
#   Have other build types point to release
#
echo "Building Sparta Infra"
cd ${GITHUB_WORKSPACE}/map/sparta
mkdir -p release
cd release
cmake .. -DCMAKE_BUILD_TYPE=$OLYMPIA_BUILD_TYPE -DGEN_DEBUG_INFO=OFF -DCMAKE_INSTALL_PREFIX=${CONDA_PREFIX}
if [ $? -ne 0 ]; then
    echo "ERROR: Cmake for Sparta framework failed"
    exit 1
fi
make -j$(nproc --all) install
BUILD_SPARTA=$?
if [ ${BUILD_SPARTA} -ne 0 ]; then
    echo "ERROR: build sparta FAILED!!!"
    exit 1
fi

cd ${GITHUB_WORKSPACE}
mkdir $OLYMPIA_BUILD_TYPE
cd $OLYMPIA_BUILD_TYPE
CXX_COMPILER=${COMPILER/clang/clang++}
CXX_COMPILER=${COMPILER/gcc/g++}

CC=$COMPILER CXX=$CXX_COMPILER cmake .. -DCMAKE_BUILD_TYPE=$OLYMPIA_BUILD_TYPE -DGEN_DEBUG_INFO=OFF
if [ $? -ne 0 ]; then
    echo "ERROR: Cmake for olympia failed"
    exit 1
fi
make -j$(nproc --all) regress
BUILD_OLYMPIA=$?
if [ ${BUILD_OLYMPIA} -ne 0 ]; then
    echo "ERROR: build/regress of olympia FAILED!!!"
    exit 1
fi
