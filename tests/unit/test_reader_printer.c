#include "quux_printer.h"
#include "quux_reader.h"
#include "quux_unit_test.h"

#include <string.h>

typedef struct test_fixture {
    quux_runtime *runtime;
    quux_context context;
} test_fixture;

static void round_trips(quux_unit_state *state, quux_context *context, const char *text,
                        const char *expected) {
    quux_value value = quux_nothing;
    QUUX_ROOT_FRAME(context, &value);
    bool read = quux_read_one(context, text, strlen(text), &value);
    QUUX_CHECK(state, read);
    if (read) {
        quux_text printed;
        quux_text_initialize(&printed);
        quux_print(context, &printed, value, QUUX_PRINT_READABLE);
        bool same = printed.bytes != NULL && strcmp(printed.bytes, expected) == 0;
        if (!same) {
            fprintf(stderr, "  read %s printed %s expected %s\n", text,
                    printed.bytes != NULL ? printed.bytes : "", expected);
        }
        QUUX_CHECK(state, same);
        quux_text_release(&printed);
    }
    QUUX_ROOT_FRAME_END(context);
}

static void literals_round_trip(quux_unit_state *state) {
    test_fixture fixture = {quux_runtime_create(), {0}};
    quux_context_initialize(&fixture.context, fixture.runtime);
    quux_context *context = &fixture.context;
    round_trips(state, context, "(1 [2 3] :key symbol \"text\")", "(1 [2 3] :key symbol \"text\")");
    round_trips(state, context, "  ; a comment\n  42  ", "42");
    round_trips(state, context, "-7", "-7");
    round_trips(state, context, "{:a 1}", "{:a 1}");
    round_trips(state, context, "#{1}", "#{1}");
    round_trips(state, context, "0.1", "0.1");
    round_trips(state, context, "1.0", "1.0");
    round_trips(state, context, "2.5e10", "2.5e+10");
    round_trips(state, context, "not-a-number", "not-a-number");
    round_trips(state, context, "negative-infinity", "negative-infinity");
    round_trips(state, context, "nothing", "nothing");
    round_trips(state, context, "\"line\\nbreak \\\"quoted\\\"\"",
                "\"line\\nbreak \\\"quoted\\\"\"");
    round_trips(state, context, "core/print-line", "core/print-line");
    round_trips(state, context, "()", "nothing");
    quux_context_finish(context);
    quux_runtime_destroy(fixture.runtime);
}

static void canonical_map_printing(quux_unit_state *state) {
    test_fixture fixture = {quux_runtime_create(), {0}};
    quux_context_initialize(&fixture.context, fixture.runtime);
    quux_context *context = &fixture.context;
    quux_value left = quux_nothing;
    quux_value right = quux_nothing;
    QUUX_ROOT_FRAME(context, &left, &right);
    QUUX_CHECK(state, quux_read_one(context, "{:a 1 :b 2 :c 3 :d 4}", 21, &left));
    QUUX_CHECK(state, quux_read_one(context, "{:d 4 :c 3 :b 2 :a 1}", 21, &right));
    quux_text first;
    quux_text second;
    quux_text_initialize(&first);
    quux_text_initialize(&second);
    quux_print(context, &first, left, QUUX_PRINT_READABLE);
    quux_print(context, &second, right, QUUX_PRINT_READABLE);
    QUUX_CHECK(state, strcmp(first.bytes, second.bytes) == 0);
    quux_text_release(&first);
    quux_text_release(&second);
    QUUX_ROOT_FRAME_END(context);
    quux_context_finish(context);
    quux_runtime_destroy(fixture.runtime);
}

static void shorthands_rejected(quux_unit_state *state) {
    test_fixture fixture = {quux_runtime_create(), {0}};
    quux_context_initialize(&fixture.context, fixture.runtime);
    quux_context *context = &fixture.context;
    quux_value value = quux_nothing;
    QUUX_ROOT_FRAME(context, &value);
    const char *rejected[] = {"'x",    "`(a b)",   "~x", "@future", "#(x)",  ")",
                              "(1 2]", "\"a\\q\"", ":",  "{:a}",    "1.2.3", "(1 2"};
    for (size_t i = 0; i < sizeof rejected / sizeof rejected[0]; i++) {
        QUUX_CHECK(state, !quux_read_one(context, rejected[i], strlen(rejected[i]), &value));
        QUUX_CHECK(state, quux_pending_error_is(context, "syntax"));
        context->pending_error = quux_nothing;
    }
    quux_reader reader;
    quux_reader_initialize(&reader, "(1 [2", 5);
    QUUX_CHECK(state, quux_reader_read(context, &reader, &value) == QUUX_READ_INCOMPLETE);
    QUUX_CHECK(state, !quux_read_one(context, "99999999999999999999", 20, &value));
    QUUX_CHECK(state, quux_pending_error_is(context, "overflow"));
    QUUX_ROOT_FRAME_END(context);
    quux_context_finish(context);
    quux_runtime_destroy(fixture.runtime);
}

QUUX_UNIT_CASES(reader_printer, {"literals round trip", literals_round_trip},
                {"equal maps print identically", canonical_map_printing},
                {"reader shorthands are rejected", shorthands_rejected});
