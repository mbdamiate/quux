/* Smoke test (T008): a correct program that uses threads, atomics, a condition variable and a
 * heap allocation. It must run without any report in every preset and under valgrind. */
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

enum { QUUX_SMOKE_THREAD_COUNT = 4, QUUX_SMOKE_INCREMENTS = 10000 };

static atomic_long quux_smoke_counter;
static pthread_mutex_t quux_smoke_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t quux_smoke_condition = PTHREAD_COND_INITIALIZER;
static int quux_smoke_finished_threads;

static void *increment_counter(void *unused) {
    (void)unused;
    for (int i = 0; i < QUUX_SMOKE_INCREMENTS; i++) {
        atomic_fetch_add_explicit(&quux_smoke_counter, 1, memory_order_relaxed);
    }
    pthread_mutex_lock(&quux_smoke_mutex);
    quux_smoke_finished_threads++;
    pthread_cond_signal(&quux_smoke_condition);
    pthread_mutex_unlock(&quux_smoke_mutex);
    return NULL;
}

int main(void) {
    long *values = malloc(sizeof(long) * QUUX_SMOKE_THREAD_COUNT);
    if (values == NULL) {
        return 1;
    }
    pthread_t threads[QUUX_SMOKE_THREAD_COUNT];
    for (int i = 0; i < QUUX_SMOKE_THREAD_COUNT; i++) {
        if (pthread_create(&threads[i], NULL, increment_counter, NULL) != 0) {
            free(values);
            return 1;
        }
    }
    pthread_mutex_lock(&quux_smoke_mutex);
    while (quux_smoke_finished_threads < QUUX_SMOKE_THREAD_COUNT) {
        pthread_cond_wait(&quux_smoke_condition, &quux_smoke_mutex);
    }
    pthread_mutex_unlock(&quux_smoke_mutex);
    for (int i = 0; i < QUUX_SMOKE_THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
        values[i] = atomic_load_explicit(&quux_smoke_counter, memory_order_relaxed);
    }
    long expected = (long)QUUX_SMOKE_THREAD_COUNT * QUUX_SMOKE_INCREMENTS;
    int status = values[QUUX_SMOKE_THREAD_COUNT - 1] == expected ? 0 : 1;
    printf("counter %ld, expected %ld\n", values[QUUX_SMOKE_THREAD_COUNT - 1], expected);
    free(values);
    return status;
}
