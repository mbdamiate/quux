/* quux-test: the test tool (D-46, contracts/command-line.md).
 *   quux-test unit [MODULE]       C unit tests
 *   quux-test examples PATH       specification examples (;=> and ;!!)
 *   quux-test program DIRECTORY   one reference program (from User Story 3) */
#include "quux_unit_test.h"

#include <stdio.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr, "usage: quux-test unit [MODULE]\n"
                    "       quux-test examples PATH\n"
                    "       quux-test program DIRECTORY\n");
    return 2;
}

int main(int argument_count, char **arguments) {
    if (argument_count < 2) {
        return usage();
    }
    if (strcmp(arguments[1], "unit") == 0) {
        int failures = quux_unit_run(argument_count > 2 ? arguments[2] : NULL);
        if (failures < 0) {
            fprintf(stderr, "quux-test: no unit test module named %s\n", arguments[2]);
            return 2;
        }
        return failures == 0 ? 0 : 1;
    }
    return usage();
}
