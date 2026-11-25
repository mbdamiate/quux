/* Smoke test (T009): a deliberate heap overflow. The test passes only when the address
 * sanitizer reports it (preset asan-ubsan). */
#include <stdio.h>
#include <stdlib.h>

int main(int argument_count, char **argument_values) {
    (void)argument_values;
    char *buffer = malloc(8);
    if (buffer == NULL) {
        return 1;
    }
    /* The index depends on the argument count so the compiler cannot prove the overflow. */
    volatile int index = 7 + argument_count;
    buffer[index] = 'x';
    printf("wrote at %d\n", index);
    free(buffer);
    return 0;
}
