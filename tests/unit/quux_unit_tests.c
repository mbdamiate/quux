/* The table of C unit test modules run by `quux-test unit [MODULE]` (T016). */
#include "quux_unit_test.h"

#include <string.h>

extern const quux_unit_module quux_unit_module_portability;
extern const quux_unit_module quux_unit_module_memory;
extern const quux_unit_module quux_unit_module_values;
extern const quux_unit_module quux_unit_module_collections;
extern const quux_unit_module quux_unit_module_reader_printer;
extern const quux_unit_module quux_unit_module_expander;
extern const quux_unit_module quux_unit_module_evaluator;
extern const quux_unit_module quux_unit_module_concurrency;
extern const quux_unit_module quux_unit_module_harness;

static const quux_unit_module *const modules[] = {
    &quux_unit_module_portability, &quux_unit_module_memory,         &quux_unit_module_values,
    &quux_unit_module_collections, &quux_unit_module_reader_printer, &quux_unit_module_expander,
    &quux_unit_module_evaluator,   &quux_unit_module_concurrency,    &quux_unit_module_harness,
};

int quux_unit_run(const char *module_name) {
    int failures = 0;
    bool found = false;
    for (size_t i = 0; i < sizeof modules / sizeof modules[0]; i++) {
        const quux_unit_module *module = modules[i];
        if (module_name != NULL && strcmp(module_name, module->name) != 0) {
            continue;
        }
        found = true;
        quux_unit_state state = {0, 0, NULL};
        for (int j = 0; j < module->case_count; j++) {
            state.current_test = module->cases[j].name;
            module->cases[j].function(&state);
        }
        printf("%-14s %3d checks, %d failed\n", module->name, state.checks, state.failures);
        failures += state.failures;
    }
    return found ? failures : -1;
}
