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
# Double check if which hash we have
# FIXME: hacky: ensure we have master here on the branch
git checkout master
echo "sparta git @ $(git rev-parse HEAD)"
mkdir -p release  # Link step expects "release" as dir name
ln -s release fastdebug
ln -s release debug
cd release
cmake .. -DCMAKE_BUILD_TYPE=$OLYMPIA_BUILD_TYPE -DGEN_DEBUG_INFO=OFF -DCMAKE_INSTALL_PREFIX=${GITHUB_WORKSPACE}/sparta_installed
if [ $? -ne 0 ]; then
    echo "ERROR: Cmake for Sparta framework failed"
    exit 1
fi
make -j2 install
BUILD_SPARTA=$?
if [ ${BUILD_SPARTA} -ne 0 ]; then
    echo "ERROR: build sparta FAILED!!!"
    exit 1
fi

cd ${GITHUB_WORKSPACE}
mkdir $OLYMPIA_BUILD_TYPE
cd $OLYMPIA_BUILD_TYPE
cmake .. -DCMAKE_BUILD_TYPE=$OLYMPIA_BUILD_TYPE -DGEN_DEBUG_INFO=OFF -DSPARTA_SEARCH_DIR=${GITHUB_WORKSPACE}/sparta_installed
if [ $? -ne 0 ]; then
    echo "ERROR: Cmake for olympia failed"
    exit 1
fi
make -j2 regress
BUILD_OLYMPIA=$?
if [ ${BUILD_OLYMPIA} -ne 0 ]; then
    echo "ERROR: build/regress of olympia FAILED!!!"
    exit 1
fi
