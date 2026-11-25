# Linux arm64 development container for quux (D-30): Ubuntu 26.04 LTS, native on the M2.
# The source is mounted at /work; the build tree lives in a named volume (run-preset.sh).
FROM ubuntu:26.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install --yes --no-install-recommends \
      clang-22 libclang-rt-22-dev clang-format-22 clang-tidy-22 \
      gcc libc6-dev cmake ninja-build valgrind git ca-certificates \
 && rm -rf /var/lib/apt/lists/*

ENV QUUX_C_COMPILER=clang-22
WORKDIR /work
