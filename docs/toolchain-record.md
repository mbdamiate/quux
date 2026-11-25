# Toolchain record

Every adjustment needed to make a build preset or a tool work on a target, with its reason and
evidence (spec FR-004, SC-008; decisions D-29..D-31, D-53). The fields follow the "Toolchain
record" entity of [data-model.md](../specs/002-blocking-prototype/data-model.md). Sanitizer
suppressions are listed here too; there must be no unexplained one.

## Adjustments

| Target | Preset | Tool | Adjustment | Reason | Evidence | Date |
|---|---|---|---|---|---|---|
| macOS arm64 | asan-ubsan | AddressSanitizer | Defect test runs with `ASAN_OPTIONS=abort_on_error=0` | On Darwin, ASan aborts after a report by default; CTest counts an abort as a failure even when the expected report matched | Without it: `smoke_heap_overflow_reported (Subprocess aborted)`; with it the process exits 1 and the test passes | 2026-11-25 |
| macOS arm64 | tsan | ThreadSanitizer | Defect test runs with `TSAN_OPTIONS=abort_on_error=0` | Same Darwin default as above | Without it: `smoke_data_race_reported (Subprocess aborted)`; with it the process exits 66 | 2026-11-25 |
| Linux arm64 (container) | all | Docker | **None needed.** No `--cap-add SYS_PTRACE`, no seccomp change, no sysctl change | Expected adjustments from research D-30 were not required: LSan reports leaks and TSan runs under Docker's default seccomp profile, because the Docker Desktop VM kernel (7.0.14-linuxkit) has `vm.mmap_rnd_bits = 18`, below TSan's limit | `smoke_memory_leak_reported` passed ("LeakSanitizer: detected memory leaks"); `smoke_data_race_reported` passed; `/proc/sys/vm/mmap_rnd_bits` = 18 | 2026-11-25 |
| macOS arm64 | asan-ubsan | LeakSanitizer | Leak test not registered on macOS | LSan is not reliable on macOS arm64 (base toolchain); leaks are checked on Linux by LSan and valgrind | — | 2026-11-25 |

## Smoke test results

| Target | Compiler | debug | asan-ubsan | tsan | release | valgrind | Date |
|---|---|---|---|---|---|---|---|
| macOS 27.0.1 arm64 (M2) | Homebrew clang 22.1.8 (`llvm@22`) | pass | pass (overflow reported) | pass (race reported) | pass | not available on macOS | 2026-11-25 |
| macOS 27.0.1 arm64 (M2) | Apple clang 21.0.0 (smoke build) | pass | — | — | — | — | 2026-11-25 |
| Linux arm64, Ubuntu 26.04.1 LTS in Docker 29.9.2 (kernel 7.0.14-linuxkit) | Ubuntu clang 22.1.2 | pass | pass (overflow and leak reported; clean program has no leak) | pass (race reported) | pass | pass (3.26.0: clean program clean, leak reported) | 2026-11-25 |
| Linux arm64, same container | gcc 15.2.0 (`debug`, warnings as errors) | pass | — | — | — | pass | 2026-11-25 |

## Presets unavailable in the container

The pre-push check (D-53) skips, with a warning, the presets listed on the line below. Keep the
line format: `container-unavailable-presets:` followed by preset names separated by spaces.

container-unavailable-presets:
