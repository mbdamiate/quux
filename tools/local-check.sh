#!/bin/sh
# Local check of the arm64 targets (D-53): every preset on macOS arm64 and in the Linux arm64
# container, with every test except those labelled `pending`. CI covers only Linux x86_64 (D-31),
# so this script is what the pre-push hook runs.
#
#   tools/local-check.sh                       full check (every preset, plus a gcc debug build in the container)
#   tools/local-check.sh --repeat N            also run the `concurrent` tests N times under tsan
#   tools/local-check.sh --documentation-only  only the name check (no C or .qx change)
#
# One summary line per target and preset is printed; the full output goes to
# build/local-check.log. Presets listed after `container-unavailable-presets:` in
# docs/toolchain-record.md are skipped in the container with a warning.
set -u

repository_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repository_root" || exit 2

repeat_count=0
documentation_only=0
while [ "$#" -gt 0 ]; do
    case "$1" in
        --repeat)
            repeat_count=${2:?--repeat needs a number}
            shift 2
            ;;
        --documentation-only)
            documentation_only=1
            shift
            ;;
        *)
            echo "usage: $0 [--repeat N] [--documentation-only]" >&2
            exit 2
            ;;
    esac
done

if [ "$(uname -s)" != "Darwin" ]; then
    echo "local-check.sh runs on the macOS development machine (it drives the container itself)" >&2
    exit 2
fi

presets="debug asan-ubsan tsan release"
mkdir -p build
log_file=build/local-check.log
: >"$log_file"
failures=0

report() {
    printf '%-22s %-11s %s\n' "$1" "$2" "$3"
}

run_logged() {
    echo "### $*" >>"$log_file"
    "$@" >>"$log_file" 2>&1
}

if [ "$documentation_only" -eq 1 ]; then
    if run_logged cmake --preset debug && run_logged ctest --preset debug --label-regex names; then
        report "macos-arm64" "debug" "pass (names only)"
        exit 0
    fi
    report "macos-arm64" "debug" "FAIL (names only, see $log_file)"
    exit 1
fi

# macOS arm64
for preset in $presets; do
    if run_logged cmake --preset "$preset" &&
        run_logged cmake --build --preset "$preset" &&
        run_logged ctest --preset "$preset" --label-exclude pending; then
        result="pass"
        if [ "$preset" = "tsan" ] && [ "$repeat_count" -gt 1 ]; then
            if run_logged ctest --preset tsan --label-regex concurrent --label-exclude pending \
                --repeat "until-fail:$repeat_count"; then
                result="pass (concurrent x$repeat_count)"
            else
                result="FAIL (concurrent x$repeat_count)"
                failures=$((failures + 1))
            fi
        fi
    else
        result="FAIL"
        failures=$((failures + 1))
    fi
    report "macos-arm64" "$preset" "$result"
done

# Linux arm64 in the container
if ! docker info >/dev/null 2>&1; then
    report "linux-arm64-container" "all" "FAIL (Docker is not running; start Docker Desktop)"
    failures=$((failures + 1))
else
    if ! docker image inspect quux-development:latest >/dev/null 2>&1; then
        run_logged tools/container/build-image.sh
    fi
    unavailable=$(sed -n 's/^container-unavailable-presets:[[:space:]]*//p' docs/toolchain-record.md)
    for preset in $presets; do
        case " $unavailable " in
            *" $preset "*)
                report "linux-arm64-container" "$preset" "SKIPPED (unavailable, see docs/toolchain-record.md)"
                continue
                ;;
        esac
        if [ "$preset" = "tsan" ] && [ "$repeat_count" -gt 1 ]; then
            container_command="ctest --preset tsan --label-exclude pending && ctest --preset tsan --label-regex concurrent --label-exclude pending --repeat until-fail:$repeat_count"
        else
            container_command="ctest --preset $preset --label-exclude pending"
        fi
        if run_logged tools/container/run-preset.sh "$preset" sh -c "$container_command"; then
            report "linux-arm64-container" "$preset" "pass"
        else
            report "linux-arm64-container" "$preset" "FAIL"
            failures=$((failures + 1))
        fi
    done
    # gcc catches warnings that clang does not (D-29); CI runs the same build on x86_64.
    if run_logged docker run --rm --volume "$repository_root:/work" \
        --volume quux-container-build:/work/build --workdir /work \
        --env QUUX_C_COMPILER=gcc quux-development:latest sh -c \
        'cmake -S . -B build/gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug >/dev/null &&
         cmake --build build/gcc-debug && ctest --test-dir build/gcc-debug --label-exclude pending'; then
        report "linux-arm64-container" "gcc debug" "pass"
    else
        report "linux-arm64-container" "gcc debug" "FAIL"
        failures=$((failures + 1))
    fi
fi

if [ "$failures" -gt 0 ]; then
    echo "local check: $failures failure(s); full output in $log_file"
    exit 1
fi
echo "local check: all arm64 presets pass"
exit 0
