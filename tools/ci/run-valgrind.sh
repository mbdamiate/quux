#!/bin/sh
# Runs the valgrind-labelled tests of a configured build tree (Linux only; valgrind does not run on
# macOS arm64).   tools/ci/run-valgrind.sh [BUILD_DIRECTORY]   (default: build/debug)
set -eu
build_directory=${1:-build/debug}
if ! command -v valgrind >/dev/null 2>&1; then
    echo "valgrind not found; it runs only on Linux (container or CI)" >&2
    exit 2
fi
exec ctest --test-dir "$build_directory" --label-regex valgrind --label-exclude pending --output-on-failure
