/* Reuse: keep. The only module that includes platform headers for threads, time and entropy, and
 * the only place with conditionals on the operating system or the architecture (D-48, H7).
 * Targets: macOS arm64, Linux arm64, Linux x86_64. */
#ifndef QUUX_PORTABILITY_H
#define QUUX_PORTABILITY_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---- Compiler attributes and builtins ---- */

#if defined(__GNUC__) || defined(__clang__)
#define QUUX_LIKELY(condition) __builtin_expect(!!(condition), 1)
#define QUUX_UNLIKELY(condition) __builtin_expect(!!(condition), 0)
#define QUUX_NO_RETURN __attribute__((noreturn))
#define QUUX_PRINTF_FORMAT(format_index, first_argument)                                           \
    __attribute__((format(printf, format_index, first_argument)))
#else
#error "quux requires a GNU-compatible C compiler (clang or gcc)"
#endif

#if defined(__has_attribute)
#if __has_attribute(musttail)
#define QUUX_HAS_MUSTTAIL 1
#endif
#endif
#ifndef QUUX_HAS_MUSTTAIL
#define QUUX_HAS_MUSTTAIL 0
#endif

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define QUUX_ADDRESS_SANITIZER 1
#endif
#if __has_feature(thread_sanitizer)
#define QUUX_THREAD_SANITIZER 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__) && !defined(QUUX_ADDRESS_SANITIZER)
#define QUUX_ADDRESS_SANITIZER 1
#endif
#if defined(__SANITIZE_THREAD__) && !defined(QUUX_THREAD_SANITIZER)
#define QUUX_THREAD_SANITIZER 1
#endif
#ifndef QUUX_ADDRESS_SANITIZER
#define QUUX_ADDRESS_SANITIZER 0
#endif
#ifndef QUUX_THREAD_SANITIZER
#define QUUX_THREAD_SANITIZER 0
#endif

/* Overflow-checked 64-bit arithmetic: return true when the result overflowed. */
#define quux_add_overflows(left, right, result) __builtin_add_overflow(left, right, result)
#define quux_subtract_overflows(left, right, result) __builtin_sub_overflow(left, right, result)
#define quux_multiply_overflows(left, right, result) __builtin_mul_overflow(left, right, result)

/* ---- Assertions ---- */

QUUX_NO_RETURN void quux_assertion_failed(const char *expression, const char *file, int line,
                                          const char *function);
#ifdef NDEBUG
#define quux_assert(expression) ((void)0)
#else
#define quux_assert(expression)                                                                    \
    ((expression) ? (void)0 : quux_assertion_failed(#expression, __FILE__, __LINE__, __func__))
#endif

/* Manual memory poisoning (used by the root verifier, D-34). No-ops without the address
 * sanitizer. */
void quux_poison_memory(void *address, size_t size);
void quux_unpoison_memory(void *address, size_t size);

/* ---- Threads ---- */

typedef struct quux_thread quux_thread;
typedef void *(*quux_thread_entry)(void *argument);

/* Starts a thread with an explicit stack size (rounded up to the page size). Returns false if
 * the thread could not be created. */
bool quux_thread_start(quux_thread **thread, size_t stack_size, quux_thread_entry entry,
                       void *argument);
/* Waits for the thread to end and releases it. Returns the entry's result. */
void *quux_thread_join(quux_thread *thread);

typedef struct quux_mutex {
    _Alignas(16) unsigned char storage[128];
} quux_mutex;
typedef struct quux_condition {
    _Alignas(16) unsigned char storage[128];
} quux_condition;

void quux_mutex_initialize(quux_mutex *mutex);
void quux_mutex_destroy(quux_mutex *mutex);
void quux_mutex_lock(quux_mutex *mutex);
void quux_mutex_unlock(quux_mutex *mutex);

void quux_condition_initialize(quux_condition *condition);
void quux_condition_destroy(quux_condition *condition);
void quux_condition_wait(quux_condition *condition, quux_mutex *mutex);
/* Waits until signalled or until the monotonic clock reaches deadline_nanoseconds. Returns
 * false on timeout. Spurious wake-ups return true; callers re-check their condition. */
bool quux_condition_wait_until(quux_condition *condition, quux_mutex *mutex,
                               uint64_t deadline_nanoseconds);
void quux_condition_signal(quux_condition *condition);
void quux_condition_broadcast(quux_condition *condition);

/* ---- Atomics: explicit memory orders at every call (contracts/module-interface.md) ---- */

typedef _Atomic(uint64_t) quux_atomic_unsigned;
typedef _Atomic(bool) quux_atomic_boolean;

#define quux_atomic_load_acquire(pointer) atomic_load_explicit(pointer, memory_order_acquire)
#define quux_atomic_load_relaxed(pointer) atomic_load_explicit(pointer, memory_order_relaxed)
#define quux_atomic_store_release(pointer, value)                                                  \
    atomic_store_explicit(pointer, value, memory_order_release)
#define quux_atomic_store_relaxed(pointer, value)                                                  \
    atomic_store_explicit(pointer, value, memory_order_relaxed)
#define quux_atomic_add_relaxed(pointer, value)                                                    \
    atomic_fetch_add_explicit(pointer, value, memory_order_relaxed)
#define quux_atomic_compare_exchange_acquire_release(pointer, expected, desired)                   \
    atomic_compare_exchange_strong_explicit(pointer, expected, desired, memory_order_acq_rel,      \
                                            memory_order_acquire)

/* ---- Time, memory pages, entropy ---- */

uint64_t quux_monotonic_nanoseconds(void);
void quux_sleep_nanoseconds(uint64_t nanoseconds);
/* The page size, queried at run time (16 KiB on macOS arm64, usually 4 KiB on Linux). */
size_t quux_page_size(void);
/* Fills the buffer with random bytes from the operating system. Returns false on failure. */
bool quux_random_bytes(void *buffer, size_t size);

#endif
