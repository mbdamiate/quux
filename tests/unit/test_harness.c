/* The program mode of quux-test rejects what it does not know (T040, FR-025). The watchdog's exit
 * status 3 ends the process, so it is checked by the CTest test harness_never-ends instead. */
#include "quux_harness.h"
#include "quux_unit_test.h"

#ifndef QUUX_SOURCE_DIRECTORY
#error "QUUX_SOURCE_DIRECTORY must be defined"
#endif

static void unknown_key_is_rejected(quux_unit_state *state) {
    quux_program_options options = {NULL, true, 1, QUUX_VERIFY_OFF};
    QUUX_CHECK(state,
               quux_run_program(QUUX_SOURCE_DIRECTORY "/tests/harness/unknown-key", &options) == 2);
}

static void unknown_assertion_is_rejected(quux_unit_state *state) {
    quux_program_options options = {NULL, true, 1, QUUX_VERIFY_OFF};
    QUUX_CHECK(state, quux_run_program(QUUX_SOURCE_DIRECTORY "/tests/harness/unknown-assertion",
                                       &options) == 2);
}

static void a_known_program_runs(quux_unit_state *state) {
    quux_program_options options = {NULL, true, 1, QUUX_VERIFY_OFF};
    QUUX_CHECK(state, quux_run_program(QUUX_SOURCE_DIRECTORY "/programs/02-first-response-wins",
                                       &options) == 0);
}

QUUX_UNIT_CASES(harness, {"an unknown key in conditions.qx is rejected", unknown_key_is_rejected},
                {"an unknown assertion in expected.qx is rejected", unknown_assertion_is_rejected},
                {"a reference program runs and passes", a_known_program_runs});
