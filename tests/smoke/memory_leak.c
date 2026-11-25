/* Smoke test (T009): a deliberate memory leak. The test passes only when LeakSanitizer (preset
 * asan-ubsan on Linux) or valgrind (preset debug on Linux) reports it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void allocate_and_forget(void) {
    char *forgotten = malloc(64);
    if (forgotten != NULL) {
        strcpy(forgotten, "never freed");
        printf("%s\n", forgotten);
    }
}

int main(void) {
    allocate_and_forget();
    return 0;
}
