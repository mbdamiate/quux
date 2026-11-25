#!/bin/sh
# Installs the Linux toolchain of D-29/D-30 on a CI runner (Ubuntu 26.04): the same packages as
# tools/container/Dockerfile.
set -eu
sudo apt-get update
sudo apt-get install --yes --no-install-recommends \
    clang-22 libclang-rt-22-dev clang-format-22 clang-tidy-22 \
    gcc libc6-dev cmake ninja-build valgrind
clang-22 --version | head -n 1
gcc --version | head -n 1
cmake --version | head -n 1
valgrind --version
