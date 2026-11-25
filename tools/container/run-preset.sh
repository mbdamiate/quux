#!/bin/sh
# Runs a preset inside the Linux arm64 container (D-30).
#   tools/container/run-preset.sh PRESET [COMMAND ...]
# Without COMMAND: configure, build and run the tests of PRESET, excluding the `pending` label.
# With COMMAND: configure and build PRESET, then run COMMAND inside /work.
# The source is mounted at /work; build trees live in the named volume quux-container-build, so
# macOS and Linux never share a build tree. Container flags needed by the sanitizers are listed,
# with their reasons, in docs/toolchain-record.md.
set -eu
if [ "$#" -lt 1 ]; then
    echo "usage: $0 PRESET [COMMAND ...]" >&2
    exit 2
fi
preset=$1
shift
repository_root=$(cd "$(dirname "$0")/../.." && pwd)

# Adjustments recorded in docs/toolchain-record.md.
container_options=${QUUX_CONTAINER_OPTIONS:-}

if [ "$#" -eq 0 ]; then
    set -- ctest --preset "$preset" --label-exclude pending
fi

# shellcheck disable=SC2086
exec docker run --rm $container_options \
    --volume "$repository_root:/work" \
    --volume quux-container-build:/work/build \
    --workdir /work \
    quux-development:latest \
    sh -c 'cmake --preset "$0" >/dev/null && cmake --build --preset "$0" && exec "$@"' "$preset" "$@"
