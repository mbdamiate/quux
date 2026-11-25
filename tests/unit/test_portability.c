#include "quux_portability.h"
#include "quux_unit_test.h"

#include <string.h>

static quux_atomic_unsigned shared_counter;

static void *count_to_thousand(void *argument) {
    (void)argument;
    for (int i = 0; i < 1000; i++) {
        quux_atomic_add_relaxed(&shared_counter, 1);
    }
    return argument;
}

static void threads_start_and_join(quux_unit_state *state) {
    quux_thread *threads[4];
    quux_atomic_store_relaxed(&shared_counter, 0);
    for (int i = 0; i < 4; i++) {
        QUUX_CHECK(state, quux_thread_start(&threads[i], 256 * 1024, count_to_thousand, NULL));
    }
    for (int i = 0; i < 4; i++) {
        quux_thread_join(threads[i]);
    }
    QUUX_CHECK(state, quux_atomic_load_relaxed(&shared_counter) == 4000);
}

static void timed_wait_times_out(quux_unit_state *state) {
    quux_mutex mutex;
    quux_condition condition;
    quux_mutex_initialize(&mutex);
    quux_condition_initialize(&condition);
    uint64_t start = quux_monotonic_nanoseconds();
    quux_mutex_lock(&mutex);
    bool signalled = true;
    while (signalled) {
        signalled = quux_condition_wait_until(&condition, &mutex, start + 20000000u);
    }
    quux_mutex_unlock(&mutex);
    uint64_t elapsed = quux_monotonic_nanoseconds() - start;
    QUUX_CHECK(state, elapsed >= 20000000u);
    QUUX_CHECK(state, elapsed < 2000000000u);
    quux_condition_destroy(&condition);
    quux_mutex_destroy(&mutex);
}

static void page_size_queried(quux_unit_state *state) {
    size_t page = quux_page_size();
    QUUX_CHECK(state, page >= 4096);
    QUUX_CHECK(state, (page & (page - 1)) == 0);
}

static void random_identifiers_differ(quux_unit_state *state) {
    unsigned char first[16];
    unsigned char second[16];
    QUUX_CHECK(state, quux_random_bytes(first, sizeof first));
    QUUX_CHECK(state, quux_random_bytes(second, sizeof second));
    QUUX_CHECK(state, memcmp(first, second, sizeof first) != 0);
}

QUUX_UNIT_CASES(portability, {"threads start and join", threads_start_and_join},
                {"timed wait times out on the monotonic clock", timed_wait_times_out},
                {"page size queried at run time", page_size_queried},
                {"128-bit random identifiers differ", random_identifiers_differ});
