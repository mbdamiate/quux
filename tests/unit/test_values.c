#include "quux_unit_test.h"
#include "quux_values.h"

#include <string.h>

typedef struct test_fixture {
    quux_runtime *runtime;
    quux_context context;
} test_fixture;

static void open_fixture(test_fixture *fixture) {
    fixture->runtime = quux_runtime_create();
    quux_context_initialize(&fixture->context, fixture->runtime);
}

static void close_fixture(test_fixture *fixture) {
    quux_context_finish(&fixture->context);
    quux_runtime_destroy(fixture->runtime);
}

static void integers_small_and_boxed(quux_unit_state *state) {
    test_fixture fixture;
    open_fixture(&fixture);
    quux_value number = quux_nothing;
    QUUX_CHECK(state, quux_make_integer(&fixture.context, 42, &number));
    QUUX_CHECK(state, quux_is_small_integer(number));
    QUUX_CHECK(state, quux_integer_value(number) == 42);
    QUUX_CHECK(state, quux_make_integer(&fixture.context, -7, &number));
    QUUX_CHECK(state, quux_integer_value(number) == -7);
    QUUX_CHECK(state, quux_make_integer(&fixture.context, INT64_MAX, &number));
    QUUX_CHECK(state, !quux_is_small_integer(number));
    QUUX_CHECK(state, quux_kind_of(number) == QUUX_KIND_INTEGER);
    QUUX_CHECK(state, quux_integer_value(number) == INT64_MAX);
    QUUX_CHECK(state, quux_make_integer(&fixture.context, INT64_MIN, &number));
    QUUX_CHECK(state, quux_integer_value(number) == INT64_MIN);
    close_fixture(&fixture);
}

static void floats_are_boxed(quux_unit_state *state) {
    test_fixture fixture;
    open_fixture(&fixture);
    quux_value number = quux_nothing;
    QUUX_CHECK(state, quux_make_float(&fixture.context, 2.5, &number));
    QUUX_CHECK(state, quux_kind_of(number) == QUUX_KIND_FLOAT);
    QUUX_CHECK(state, quux_float_value(number) == 2.5);
    close_fixture(&fixture);
}

static void symbols_and_keywords_are_interned(quux_unit_state *state) {
    test_fixture fixture;
    open_fixture(&fixture);
    quux_value first = quux_symbol(&fixture.context, "print-line");
    quux_value second = quux_symbol(&fixture.context, "print-line");
    quux_value keyword = quux_keyword(&fixture.context, "print-line");
    QUUX_CHECK(state, first == second);
    QUUX_CHECK(state, first != keyword);
    QUUX_CHECK(state, quux_kind_of(keyword) == QUUX_KIND_KEYWORD);
    quux_value qualified = quux_symbol(&fixture.context, "core/print-line");
    QUUX_CHECK(state, quux_symbol_namespace_length(qualified) == 4);
    QUUX_CHECK(state, quux_symbol_namespace_length(quux_symbol(&fixture.context, "/")) == 0);
    for (int i = 0; i < 2000; i++) { /* forces the table to grow */
        char name[32];
        snprintf(name, sizeof name, "name-%d", i);
        (void)quux_symbol(&fixture.context, name);
    }
    QUUX_CHECK(state, quux_symbol(&fixture.context, "print-line") == first);
    quux_value unique = quux_nothing;
    QUUX_CHECK(state, quux_make_unique_symbol(&fixture.context, "temporary", 9, &unique));
    QUUX_CHECK(state, quux_kind_of(unique) == QUUX_KIND_SYMBOL);
    QUUX_CHECK(state, unique != quux_symbol(&fixture.context, "temporary"));
    close_fixture(&fixture);
}

static void errors_are_values(quux_unit_state *state) {
    test_fixture fixture;
    open_fixture(&fixture);
    QUUX_CHECK(state, !quux_raise(&fixture.context, "overflow", "%d too large", 7));
    QUUX_CHECK(state, quux_pending_error_is(&fixture.context, "overflow"));
    quux_value message = quux_error_field(fixture.context.pending_error, QUUX_ERROR_MESSAGE);
    QUUX_CHECK(state, strcmp(quux_string_bytes(message), "7 too large") == 0);
    close_fixture(&fixture);
}

QUUX_UNIT_CASES(values, {"integers are small or boxed", integers_small_and_boxed},
                {"floats are boxed", floats_are_boxed},
                {"symbols and keywords are interned", symbols_and_keywords_are_interned},
                {"errors are values", errors_are_values});
