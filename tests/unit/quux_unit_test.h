/* Minimal assertion macros for the C unit tests (T016, D-46). Each test file defines test
 * functions and a module table entry in quux_unit_tests.c; `quux-test unit [MODULE]` runs them. */
#ifndef QUUX_UNIT_TEST_H
#define QUUX_UNIT_TEST_H

#include <stdbool.h>
#include <stdio.h>

typedef struct quux_unit_state {
    int checks;
    int failures;
    const char *current_test;
} quux_unit_state;

typedef void (*quux_unit_function)(quux_unit_state *state);

typedef struct quux_unit_case {
    const char *name;
    quux_unit_function function;
} quux_unit_case;

typedef struct quux_unit_module {
    const char *name;
    const quux_unit_case *cases;
    int case_count;
} quux_unit_module;

#define QUUX_CHECK(state, condition)                                                               \
    do {                                                                                           \
        (state)->checks++;                                                                         \
        if (!(condition)) {                                                                        \
            (state)->failures++;                                                                   \
            fprintf(stderr, "  FAILED %s: %s (%s:%d)\n", (state)->current_test, #condition,        \
                    __FILE__, __LINE__);                                                           \
        }                                                                                          \
    } while (0)

#define QUUX_UNIT_CASES(module_name, ...)                                                          \
    static const quux_unit_case module_name##_cases[] = {__VA_ARGS__};                             \
    const quux_unit_module quux_unit_module_##module_name = {                                      \
        #module_name, module_name##_cases,                                                         \
        (int)(sizeof module_name##_cases / sizeof module_name##_cases[0])}

/* Runs one module (or all when name is NULL). Returns the number of failed checks, or -1 if no
 * module has that name. */
int quux_unit_run(const char *module_name);

#endif
