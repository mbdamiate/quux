#include "quux_portability.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/random.h>
#elif defined(__linux__)
#include <sys/random.h>
#else
#error "unsupported operating system: quux targets macOS and Linux (D-28)"
#endif

#if QUUX_ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h>
#endif

_Static_assert(sizeof(pthread_mutex_t) <= sizeof(quux_mutex), "quux_mutex storage too small");
_Static_assert(sizeof(pthread_cond_t) <= sizeof(quux_condition),
               "quux_condition storage too small");
_Static_assert(_Alignof(pthread_mutex_t) <= 16, "quux_mutex alignment");
_Static_assert(_Alignof(pthread_cond_t) <= 16, "quux_condition alignment");
_Static_assert(sizeof(void *) == 8, "quux requires a 64-bit target");

void quux_assertion_failed(const char *expression, const char *file, int line,
                           const char *function) {
    fprintf(stderr, "quux: assertion failed: %s (%s:%d, in %s)\n", expression, file, line,
            function);
    abort();
}

void quux_poison_memory(void *address, size_t size) {
#if QUUX_ADDRESS_SANITIZER
    __asan_poison_memory_region(address, size);
#else
    (void)address;
    (void)size;
#endif
}

void quux_unpoison_memory(void *address, size_t size) {
#if QUUX_ADDRESS_SANITIZER
    __asan_unpoison_memory_region(address, size);
#else
    (void)address;
    (void)size;
#endif
}

/* ---- Threads ---- */

struct quux_thread {
    pthread_t handle;
};

bool quux_thread_start(quux_thread **thread, size_t stack_size, quux_thread_entry entry,
                       void *argument) {
    quux_thread *created = malloc(sizeof *created);
    if (created == NULL) {
        return false;
    }
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0) {
        free(created);
        return false;
    }
    size_t page = quux_page_size();
    size_t rounded = (stack_size + page - 1) / page * page;
    if (rounded < (size_t)PTHREAD_STACK_MIN) {
        rounded = (size_t)PTHREAD_STACK_MIN;
    }
    bool started = pthread_attr_setstacksize(&attributes, rounded) == 0 &&
                   pthread_create(&created->handle, &attributes, entry, argument) == 0;
    pthread_attr_destroy(&attributes);
    if (!started) {
        free(created);
        return false;
    }
    *thread = created;
    return true;
}

void *quux_thread_join(quux_thread *thread) {
    void *result = NULL;
    pthread_join(thread->handle, &result);
    free(thread);
    return result;
}

static pthread_mutex_t *native_mutex(quux_mutex *mutex) {
    return (pthread_mutex_t *)(void *)mutex->storage;
}

static pthread_cond_t *native_condition(quux_condition *condition) {
    return (pthread_cond_t *)(void *)condition->storage;
}

void quux_mutex_initialize(quux_mutex *mutex) {
    int status = pthread_mutex_init(native_mutex(mutex), NULL);
    quux_assert(status == 0);
    (void)status;
}

void quux_mutex_destroy(quux_mutex *mutex) {
    pthread_mutex_destroy(native_mutex(mutex));
}

void quux_mutex_lock(quux_mutex *mutex) {
    int status = pthread_mutex_lock(native_mutex(mutex));
    quux_assert(status == 0);
    (void)status;
}

void quux_mutex_unlock(quux_mutex *mutex) {
    int status = pthread_mutex_unlock(native_mutex(mutex));
    quux_assert(status == 0);
    (void)status;
}

/* macOS has no pthread_condattr_setclock, so its condition variables cannot wait on the
 * monotonic clock directly. There, the monotonic deadline is turned into a relative wait
 * (pthread_cond_timedwait_relative_np), which does not depend on the wall clock. On Linux the
 * condition variable is created with CLOCK_MONOTONIC. */
void quux_condition_initialize(quux_condition *condition) {
#if defined(__APPLE__)
    int status = pthread_cond_init(native_condition(condition), NULL);
#else
    pthread_condattr_t attributes;
    pthread_condattr_init(&attributes);
    pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
    int status = pthread_cond_init(native_condition(condition), &attributes);
    pthread_condattr_destroy(&attributes);
#endif
    quux_assert(status == 0);
    (void)status;
}

void quux_condition_destroy(quux_condition *condition) {
    pthread_cond_destroy(native_condition(condition));
}

void quux_condition_wait(quux_condition *condition, quux_mutex *mutex) {
    pthread_cond_wait(native_condition(condition), native_mutex(mutex));
}

bool quux_condition_wait_until(quux_condition *condition, quux_mutex *mutex,
                               uint64_t deadline_nanoseconds) {
    uint64_t now = quux_monotonic_nanoseconds();
    if (now >= deadline_nanoseconds) {
        return false;
    }
#if defined(__APPLE__)
    uint64_t remaining = deadline_nanoseconds - now;
    struct timespec relative = {.tv_sec = (time_t)(remaining / 1000000000u),
                                .tv_nsec = (long)(remaining % 1000000000u)};
    int status = pthread_cond_timedwait_relative_np(native_condition(condition),
                                                    native_mutex(mutex), &relative);
#else
    struct timespec absolute = {.tv_sec = (time_t)(deadline_nanoseconds / 1000000000u),
                                .tv_nsec = (long)(deadline_nanoseconds % 1000000000u)};
    int status =
        pthread_cond_timedwait(native_condition(condition), native_mutex(mutex), &absolute);
#endif
    return status != ETIMEDOUT;
}

void quux_condition_signal(quux_condition *condition) {
    pthread_cond_signal(native_condition(condition));
}

void quux_condition_broadcast(quux_condition *condition) {
    pthread_cond_broadcast(native_condition(condition));
}

/* ---- Time, pages, entropy ---- */

uint64_t quux_monotonic_nanoseconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000u + (uint64_t)now.tv_nsec;
}

void quux_sleep_nanoseconds(uint64_t nanoseconds) {
    struct timespec request = {.tv_sec = (time_t)(nanoseconds / 1000000000u),
                               .tv_nsec = (long)(nanoseconds % 1000000000u)};
    while (nanosleep(&request, &request) != 0 && errno == EINTR) {
    }
}

size_t quux_page_size(void) {
    long size = sysconf(_SC_PAGESIZE);
    return size > 0 ? (size_t)size : 4096u;
}

bool quux_random_bytes(void *buffer, size_t size) {
    unsigned char *bytes = buffer;
    while (size > 0) {
        size_t chunk = size > 256 ? 256 : size; /* getentropy accepts at most 256 bytes */
        if (getentropy(bytes, chunk) != 0) {
            return false;
        }
        bytes += chunk;
        size -= chunk;
    }
    return true;
}
