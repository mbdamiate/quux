/* Smoke test (T009): a deliberate data race on a plain integer. The test passes only when the
 * thread sanitizer reports it (preset tsan). */
#include <pthread.h>
#include <stdio.h>

static long quux_smoke_shared_counter;

static void *increment_without_synchronization(void *unused) {
    (void)unused;
    for (int i = 0; i < 1000; i++) {
        quux_smoke_shared_counter++;
    }
    return NULL;
}

int main(void) {
    pthread_t first_thread;
    pthread_t second_thread;
    if (pthread_create(&first_thread, NULL, increment_without_synchronization, NULL) != 0 ||
        pthread_create(&second_thread, NULL, increment_without_synchronization, NULL) != 0) {
        return 1;
    }
    pthread_join(first_thread, NULL);
    pthread_join(second_thread, NULL);
    printf("counter %ld\n", quux_smoke_shared_counter);
    return 0;
}
