/* The program mode of quux-test (D-46, contracts/harness-report.md, the reference-program
 * contract of feature 001): reads conditions.qx and expected.qx as quux data, rejects unknown
 * keys and assertion forms (FR-025), injects the delays and failures of `tag`, runs `main` under a
 * watchdog, evaluates every assertion and prints a report (data-model.md "Program run"). */
#include "quux_harness.h"

#include "quux_collections.h"
#include "quux_namespaces.h"
#include "quux_printer.h"
#include "quux_reader.h"
#include "quux_session.h"

#include <stdlib.h>
#include <string.h>

#ifndef QUUX_TARGET_NAME
#define QUUX_TARGET_NAME "unknown-target"
#endif
#ifndef QUUX_TARGET_SYSTEM
#define QUUX_TARGET_SYSTEM "unknown"
#endif
#ifndef QUUX_TARGET_ARCHITECTURE
#define QUUX_TARGET_ARCHITECTURE "unknown"
#endif
#ifndef QUUX_PRESET_NAME
#define QUUX_PRESET_NAME "unknown"
#endif

enum {
    STATUS_PASS = 0,
    STATUS_FAIL = 1,
    STATUS_INVALID = 2,
    STATUS_TIMEOUT = 3,
};

/* data-model.md "Program run". The quux values are global roots of the run's runtime. */
typedef struct program_run {
    const char *directory;
    const quux_program_options *options;
    const char *name;
    char *main_text;
    size_t main_length;
    char *conditions_text;
    size_t conditions_length;
    char *expected_text;
    size_t expected_length;
    uint64_t seed;
    uint64_t bound_milliseconds;
    uint64_t elapsed_milliseconds; /* Program run.elapsed_milliseconds */
    bool raised;
    bool sequential_raised;
    bool needs_sequential;
    int status;
    /* Program run.conditions and .assertions */
    quux_value conditions;
    quux_value assertions;
    quux_value delays;
    quux_value failures;
    quux_value input;
    /* Program run.outcome: the value returned by main, or the error it raised */
    quux_value outcome;
    quux_value sequential_outcome;
    /* Program run.tag_events: a list of [tag future] (future is nothing outside futures) */
    quux_value tag_events;
    quux_value report_entries;
    quux_mutex tag_mutex;
    /* watchdog */
    quux_mutex watchdog_mutex;
    quux_condition watchdog_condition;
    bool finished;
} program_run;

static program_run *current_run; /* one program per process */

static void invalid(program_run *run, const char *format, const char *detail) {
    printf("program %s\n  invalid ", run->name);
    printf(format, detail);
    printf("\n");
    run->status = STATUS_INVALID;
}

static bool is_non_negative_integer(quux_value value) {
    return quux_is_kind(value, QUUX_KIND_INTEGER) && quux_integer_value(value) >= 0;
}

static const char *keyword_text(quux_value keyword, char *buffer, size_t size) {
    snprintf(buffer, size, ":%.*s", (int)quux_name_length(keyword), quux_name_bytes(keyword));
    return buffer;
}

/* ---- conditions.qx ---- */

typedef struct validation {
    program_run *run;
    quux_context *context;
    bool valid;
    const char *section; /* the key whose map is being validated */
} validation;

static bool validate_condition_key(void *data, quux_value key, quux_value value) {
    validation *check = data;
    program_run *run = check->run;
    char text[256];
    if (!quux_is_kind(key, QUUX_KIND_KEYWORD)) {
        invalid(run, "conditions.qx: a key is not a keyword%s", "");
        return check->valid = false;
    }
    keyword_text(key, text, sizeof text);
    if (quux_name_equals(key, "input")) {
        run->input = value;
    } else if (quux_name_equals(key, "delays") || quux_name_equals(key, "fail") ||
               quux_name_equals(key, "bounds")) {
        if (!quux_is_kind(value, QUUX_KIND_MAP)) {
            invalid(run, "conditions.qx: %s must be a map", text);
            return check->valid = false;
        }
        if (quux_name_equals(key, "delays")) {
            run->delays = value;
        } else if (quux_name_equals(key, "fail")) {
            run->failures = value;
        }
    } else if (quux_name_equals(key, "nodes") || quux_name_equals(key, "network") ||
               quux_name_equals(key, "files")) {
        invalid(run, "conditions.qx: %s is not supported by this harness yet", text);
        return check->valid = false;
    } else {
        invalid(run, "conditions.qx: unknown key %s", text);
        return check->valid = false;
    }
    return true;
}

static bool validate_delay(void *data, quux_value key, quux_value value) {
    validation *check = data;
    if (!quux_is_kind(key, QUUX_KIND_KEYWORD) || !is_non_negative_integer(value)) {
        invalid(check->run, "conditions.qx: %s maps tags to milliseconds", ":delays");
        return check->valid = false;
    }
    return true;
}

static bool validate_failure_key(void *data, quux_value key, quux_value value) {
    validation *check = data;
    bool known = quux_is_kind(key, QUUX_KIND_KEYWORD) &&
                 ((quux_name_equals(key, "kind") && quux_is_kind(value, QUUX_KIND_KEYWORD)) ||
                  (quux_name_equals(key, "after-milliseconds") && is_non_negative_integer(value)));
    if (!known) {
        char text[256];
        invalid(check->run, "conditions.qx: unknown or malformed key %s in a :fail entry",
                quux_is_kind(key, QUUX_KIND_KEYWORD) ? keyword_text(key, text, sizeof text)
                                                     : "(not a keyword)");
        return check->valid = false;
    }
    return true;
}

static bool validate_failure(void *data, quux_value key, quux_value value) {
    validation *check = data;
    if (!quux_is_kind(key, QUUX_KIND_KEYWORD) || !quux_is_kind(value, QUUX_KIND_MAP)) {
        invalid(check->run, "conditions.qx: %s maps tags to {:kind k :after-milliseconds n}",
                ":fail");
        return check->valid = false;
    }
    quux_value kind = quux_nothing;
    quux_get(check->context, value, quux_keyword(check->context, "kind"), quux_nothing, &kind);
    if (!quux_is_kind(kind, QUUX_KIND_KEYWORD)) {
        invalid(check->run, "conditions.qx: a %s entry needs :kind", ":fail");
        return check->valid = false;
    }
    return quux_map_for_each(value, validate_failure_key, check) && check->valid;
}

static bool validate_bound(void *data, quux_value key, quux_value value) {
    validation *check = data;
    if (!quux_is_kind(key, QUUX_KIND_KEYWORD) ||
        !quux_name_equals(key, "wall-clock-milliseconds") || !is_non_negative_integer(value)) {
        invalid(check->run, "conditions.qx: %s has only :wall-clock-milliseconds", ":bounds");
        return check->valid = false;
    }
    check->run->bound_milliseconds = (uint64_t)quux_integer_value(value);
    return true;
}

static bool validate_conditions(quux_context *context, program_run *run) {
    if (!quux_is_kind(run->conditions, QUUX_KIND_MAP)) {
        invalid(run, "conditions.qx must hold one map%s", "");
        return false;
    }
    validation check = {run, context, true, NULL};
    quux_map_for_each(run->conditions, validate_condition_key, &check);
    if (check.valid && quux_is_kind(run->delays, QUUX_KIND_MAP)) {
        quux_map_for_each(run->delays, validate_delay, &check);
    }
    if (check.valid && quux_is_kind(run->failures, QUUX_KIND_MAP)) {
        quux_map_for_each(run->failures, validate_failure, &check);
    }
    quux_value bounds = quux_nothing;
    quux_get(context, run->conditions, quux_keyword(context, "bounds"), quux_nothing, &bounds);
    if (check.valid && quux_is_kind(bounds, QUUX_KIND_MAP)) {
        quux_map_for_each(bounds, validate_bound, &check);
    }
    return check.valid;
}

/* ---- expected.qx ---- */

typedef enum assertion_kind {
    ASSERT_RETURNS,
    ASSERT_RETURNS_SATISFYING,
    ASSERT_RAISES,
    ASSERT_RAISES_ONE_OF,
    ASSERT_SUPPRESSED_INCLUDES,
    ASSERT_FUTURE_STATE,
    ASSERT_FUTURE_ERROR,
    ASSERT_ELAPSED_UNDER,
    ASSERT_ELAPSED_AT_LEAST,
    ASSERT_SAME_AS_SEQUENTIAL,
    ASSERT_UNSUPPORTED,
    ASSERT_UNKNOWN,
} assertion_kind;

static const struct {
    const char *name;
    assertion_kind kind;
    uint64_t arguments;
} assertion_forms[] = {
    {"returns", ASSERT_RETURNS, 1},
    {"returns-satisfying", ASSERT_RETURNS_SATISFYING, 1},
    {"raises", ASSERT_RAISES, 1},
    {"raises-one-of", ASSERT_RAISES_ONE_OF, 1},
    {"suppressed-includes", ASSERT_SUPPRESSED_INCLUDES, 1},
    {"future-state", ASSERT_FUTURE_STATE, 2},
    {"future-error", ASSERT_FUTURE_ERROR, 2},
    {"elapsed-under", ASSERT_ELAPSED_UNDER, 1},
    {"elapsed-at-least", ASSERT_ELAPSED_AT_LEAST, 1},
    {"same-as-sequential", ASSERT_SAME_AS_SEQUENTIAL, 0},
    /* known forms that later user stories implement (T050, T062) */
    {"agent-final", ASSERT_UNSUPPORTED, 2},
    {"never", ASSERT_UNSUPPORTED, 1},
    {"maximum-ahead", ASSERT_UNSUPPORTED, 2},
    {"output-lines", ASSERT_UNSUPPORTED, 1},
    {"deterministic", ASSERT_UNSUPPORTED, 1},
    {"applied-once", ASSERT_UNSUPPORTED, 1},
};

static assertion_kind classify(quux_value assertion, uint64_t *expected_arguments) {
    if (!quux_is_kind(assertion, QUUX_KIND_PAIR) ||
        !quux_is_kind(quux_pair_first(assertion), QUUX_KIND_SYMBOL)) {
        return ASSERT_UNKNOWN;
    }
    quux_value head = quux_pair_first(assertion);
    for (size_t i = 0; i < sizeof assertion_forms / sizeof assertion_forms[0]; i++) {
        if (quux_name_equals(head, assertion_forms[i].name)) {
            *expected_arguments = assertion_forms[i].arguments;
            return assertion_forms[i].kind;
        }
    }
    return ASSERT_UNKNOWN;
}

static quux_value assertion_argument(quux_value assertion, uint64_t index) {
    quux_value cell = quux_pair_rest(assertion);
    for (uint64_t i = 0; i < index; i++) {
        cell = quux_pair_rest(cell);
    }
    return quux_pair_first(cell);
}

static bool validate_assertions(quux_context *context, program_run *run) {
    if (!quux_is_kind(run->assertions, QUUX_KIND_VECTOR)) {
        invalid(run, "expected.qx must hold one vector of assertions%s", "");
        return false;
    }
    uint64_t count = 0;
    quux_count(context, run->assertions, &count);
    for (uint64_t i = 0; i < count; i++) {
        quux_value assertion = quux_vector_element(run->assertions, i);
        uint64_t expected_arguments = 0;
        assertion_kind kind = classify(assertion, &expected_arguments);
        quux_text text;
        quux_text_initialize(&text);
        quux_print(context, &text, assertion, QUUX_PRINT_READABLE);
        const char *printed = text.bytes != NULL ? text.bytes : "";
        bool valid = true;
        if (kind == ASSERT_UNKNOWN) {
            invalid(run, "expected.qx: unknown assertion %s", printed);
            valid = false;
        } else if (quux_list_count(assertion) - 1 != expected_arguments) {
            invalid(run, "expected.qx: wrong number of arguments in %s", printed);
            valid = false;
        } else if (kind == ASSERT_UNSUPPORTED) {
            invalid(run, "expected.qx: %s is not supported by this harness yet", printed);
            valid = false;
        } else if (kind == ASSERT_SAME_AS_SEQUENTIAL) {
            run->needs_sequential = true;
        } else if ((kind == ASSERT_RAISES || kind == ASSERT_SUPPRESSED_INCLUDES) &&
                   !quux_is_kind(assertion_argument(assertion, 0), QUUX_KIND_KEYWORD)) {
            invalid(run, "expected.qx: %s expects an error kind", printed);
            valid = false;
        } else if ((kind == ASSERT_FUTURE_STATE || kind == ASSERT_FUTURE_ERROR) &&
                   (!quux_is_kind(assertion_argument(assertion, 0), QUUX_KIND_KEYWORD) ||
                    !quux_is_kind(assertion_argument(assertion, 1), QUUX_KIND_KEYWORD))) {
            invalid(run, "expected.qx: %s expects a tag and a keyword", printed);
            valid = false;
        } else if ((kind == ASSERT_ELAPSED_UNDER || kind == ASSERT_ELAPSED_AT_LEAST) &&
                   !is_non_negative_integer(assertion_argument(assertion, 0))) {
            invalid(run, "expected.qx: %s expects milliseconds", printed);
            valid = false;
        } else if (kind == ASSERT_RAISES_ONE_OF &&
                   !quux_is_kind(assertion_argument(assertion, 0), QUUX_KIND_VECTOR)) {
            invalid(run, "expected.qx: %s expects a vector of error kinds", printed);
            valid = false;
        }
        quux_text_release(&text);
        if (!valid) {
            return false;
        }
    }
    return true;
}

/* ---- tag (the reference-program contract's harness conventions) ---- */

static bool record_tag(quux_context *context, program_run *run, quux_value name) {
    quux_value event = quux_nothing;
    quux_value cell = quux_nothing;
    QUUX_ROOT_FRAME(context, &name, &event, &cell);
    bool done = quux_vector_empty(context, &event) &&
                quux_vector_add(context, event, name, &event) &&
                quux_vector_add(context, event, context->task_future, &event) &&
                quux_list_prepend(context, event, quux_nothing, &cell);
    if (done) {
        quux_mutex_lock(&run->tag_mutex);
        quux_object *object = quux_checked_object(cell);
        object->values[1] = run->tag_events; /* the cell is not shared yet */
        run->tag_events = cell;
        quux_mutex_unlock(&run->tag_mutex);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool primitive_harness_tag(quux_context *context, quux_value *arguments, uint32_t count,
                                  quux_value *result) {
    (void)count;
    program_run *run = current_run;
    quux_value name = arguments[0];
    quux_value function = arguments[1];
    quux_value setting = quux_nothing;
    quux_value empty = quux_nothing;
    if (!quux_is_kind(name, QUUX_KIND_KEYWORD)) {
        return quux_raise(context, "wrong-type", "tag expects a keyword, got %s",
                          quux_kind_phrase(quux_kind_of(name)));
    }
    QUUX_ROOT_FRAME(context, &name, &function, &setting, &empty);
    bool done = record_tag(context, run, name);
    if (done && quux_is_kind(run->delays, QUUX_KIND_MAP) &&
        quux_get(context, run->delays, name, quux_nothing, &setting) && !quux_is_nothing(setting)) {
        /* an injected wait before the expression, a cancellation point like sleep */
        done = quux_concurrency_sleep(context, (uint64_t)quux_integer_value(setting));
    }
    setting = quux_nothing;
    if (done && quux_is_kind(run->failures, QUUX_KIND_MAP) &&
        quux_get(context, run->failures, name, quux_nothing, &setting) &&
        !quux_is_nothing(setting)) {
        quux_value kind = quux_nothing;
        quux_value after = quux_nothing;
        quux_get(context, setting, quux_keyword(context, "kind"), quux_nothing, &kind);
        quux_get(context, setting, quux_keyword(context, "after-milliseconds"),
                 quux_small_integer(0), &after);
        done = quux_concurrency_sleep(context, (uint64_t)quux_integer_value(after));
        if (done) {
            char text[256];
            done = quux_raise(context, quux_name_bytes(kind), "injected failure at tag %s",
                              keyword_text(name, text, sizeof text));
        }
    } else if (done) {
        done =
            quux_make_arguments(context, 0, &empty) && quux_apply(context, function, empty, result);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- Watchdog (bounds :wall-clock-milliseconds) ---- */

static void *watch(void *argument) {
    program_run *run = argument;
    uint64_t deadline = quux_monotonic_nanoseconds() + run->bound_milliseconds * 1000000u;
    quux_mutex_lock(&run->watchdog_mutex);
    while (!run->finished) {
        if (!quux_condition_wait_until(&run->watchdog_condition, &run->watchdog_mutex, deadline) &&
            quux_monotonic_nanoseconds() >= deadline) {
            break;
        }
    }
    bool finished = run->finished;
    quux_mutex_unlock(&run->watchdog_mutex);
    if (!finished) {
        printf("program %s   seed %016llx   target %s %s\n  result  timeout after %llu ms\n",
               run->name, (unsigned long long)run->seed, QUUX_TARGET_NAME, QUUX_PRESET_NAME,
               (unsigned long long)run->bound_milliseconds);
        fflush(stdout);
        quux_exit_immediately(STATUS_TIMEOUT);
    }
    return NULL;
}

static void stop_watchdog(program_run *run, quux_thread *watchdog) {
    quux_mutex_lock(&run->watchdog_mutex);
    run->finished = true;
    quux_condition_signal(&run->watchdog_condition);
    quux_mutex_unlock(&run->watchdog_mutex);
    quux_thread_join(watchdog);
}

/* ---- Running main ---- */

static bool call_main(quux_context *context, quux_value scope, void *data, quux_value *result) {
    (void)scope;
    quux_value *function = data;
    return quux_call1(context, *function, current_run->input, result);
}

/* Calls (name input) in the program's namespace, inside a child of the root scope. */
static bool run_entry(quux_context *context, const char *name, quux_value *result) {
    quux_value definition = quux_nothing;
    quux_value function = quux_nothing;
    QUUX_ROOT_FRAME(context, &definition, &function);
    bool done = quux_namespace_resolve(context, context->current_namespace,
                                       quux_symbol(context, name), &definition);
    if (done && quux_is_nothing(definition)) {
        done = quux_raise(context, "undefined-name", "main.qx does not define %s", name);
    }
    if (done) {
        function = quux_definition_field(definition, QUUX_DEFINITION_VALUE);
        done = quux_concurrency_with_scope(context, call_main, &function, result);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- Assertions ---- */

typedef struct verdict {
    bool held;
    quux_value observed;     /* a quux value for the data report */
    char observed_text[512]; /* for the text report */
} verdict;

static void observe(quux_context *context, verdict *result, quux_value value) {
    result->observed = value;
    quux_text printed;
    quux_text_initialize(&printed);
    quux_print(context, &printed, value, QUUX_PRINT_READABLE);
    const char *bytes = printed.bytes != NULL ? printed.bytes : "";
    enum { SHOWN = 100 };
    if (strlen(bytes) > SHOWN) {
        snprintf(result->observed_text, sizeof result->observed_text, "%.*s ...", (int)SHOWN,
                 bytes);
    } else {
        snprintf(result->observed_text, sizeof result->observed_text, "%s", bytes);
    }
    quux_text_release(&printed);
}

static quux_value outcome_kind(program_run *run) {
    return run->raised ? quux_error_field(run->outcome, QUUX_ERROR_KIND) : quux_nothing;
}

/* The futures recorded for a tag, as a vector (nothing stands for a tag evaluated outside any
 * future). The observed values of future-state and future-error use only approved names: a
 * state or an error kind, nothing (outside any future, or no error), and [] (never evaluated). */
static bool tag_futures(quux_context *context, program_run *run, quux_value tag,
                        quux_value *result) {
    quux_value futures = quux_nothing;
    QUUX_ROOT_FRAME(context, &tag, &futures);
    bool done = quux_vector_empty(context, &futures);
    for (quux_value list = run->tag_events; done && !quux_is_nothing(list);
         list = quux_pair_rest(list)) {
        quux_value event = quux_pair_first(list);
        if (quux_vector_element(event, 0) == tag) {
            done = quux_vector_add(context, futures, quux_vector_element(event, 1), &futures);
        }
    }
    if (done) {
        *result = futures;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static void check_future_tag(quux_context *context, program_run *run, assertion_kind kind,
                             quux_value tag, quux_value wanted, verdict *result) {
    quux_value futures = quux_nothing;
    quux_value observed = quux_nothing;
    quux_value item = quux_nothing;
    QUUX_ROOT_FRAME(context, &tag, &wanted, &futures, &observed, &item);
    uint64_t count = 0;
    bool done = tag_futures(context, run, tag, &futures) && quux_count(context, futures, &count) &&
                quux_vector_empty(context, &observed);
    result->held = done && count > 0;
    quux_value last = quux_nothing;
    uint64_t distinct = 0;
    for (uint64_t i = 0; done && i < count; i++) {
        quux_value future = quux_vector_element(futures, i);
        if (!quux_is_kind(future, QUUX_KIND_FUTURE)) {
            item = quux_nothing; /* the tag was evaluated outside any future */
            result->held = false;
        } else if (kind == ASSERT_FUTURE_STATE) {
            static const char *const names[] = {"empty", "filled", "failed"};
            item = quux_keyword(context, names[quux_future_current_state(future)]);
            result->held = result->held && item == wanted;
        } else {
            quux_value error = quux_future_error(future);
            item = quux_is_nothing(error) ? quux_nothing : quux_error_field(error, QUUX_ERROR_KIND);
            result->held = result->held && item == wanted;
        }
        /* keywords are interned: one entry per run of equal observations */
        if (i == 0 || item != last) {
            done = quux_vector_add(context, observed, item, &observed);
            distinct++;
        }
        last = item;
    }
    if (distinct == 1) { /* with no instance at all, observed stays the empty vector */
        observed = quux_vector_element(observed, 0);
    }
    observe(context, result, observed);
    QUUX_ROOT_FRAME_END(context);
}

static bool suppressed_includes(quux_context *context, quux_value error, quux_value kind) {
    quux_value suppressed = quux_error_field(error, QUUX_ERROR_SUPPRESSED);
    if (!quux_is_kind(suppressed, QUUX_KIND_VECTOR)) {
        return false;
    }
    uint64_t count = 0;
    quux_count(context, suppressed, &count);
    for (uint64_t i = 0; i < count; i++) {
        quux_value item = quux_vector_element(suppressed, i);
        if (quux_is_kind(item, QUUX_KIND_ERROR) &&
            quux_error_field(item, QUUX_ERROR_KIND) == kind) {
            return true;
        }
    }
    return false;
}

static void evaluate_assertion(quux_context *context, program_run *run, quux_value assertion,
                               verdict *result) {
    uint64_t ignored = 0;
    assertion_kind kind = classify(assertion, &ignored);
    quux_value argument =
        quux_list_count(assertion) > 1 ? assertion_argument(assertion, 0) : quux_nothing;
    quux_value value = quux_nothing;
    QUUX_ROOT_FRAME(context, &assertion, &argument, &value);
    result->held = false;
    result->observed = quux_nothing;
    snprintf(result->observed_text, sizeof result->observed_text, "nothing");
    switch (kind) {
    case ASSERT_RETURNS:
        result->held = !run->raised && quux_equal(context, run->outcome, argument);
        observe(context, result, run->raised ? outcome_kind(run) : run->outcome);
        break;
    case ASSERT_RETURNS_SATISFYING:
        if (run->raised) {
            observe(context, result, outcome_kind(run));
        } else if (quux_evaluate_form(context, argument, &value) &&
                   quux_call1(context, value, run->outcome, &value)) {
            result->held = quux_is_truthy(value);
            observe(context, result, run->outcome);
        } else {
            observe(context, result, quux_error_field(context->pending_error, QUUX_ERROR_KIND));
            context->pending_error = quux_nothing;
        }
        break;
    case ASSERT_RAISES:
        result->held = run->raised && outcome_kind(run) == argument;
        observe(context, result, run->raised ? outcome_kind(run) : run->outcome);
        break;
    case ASSERT_RAISES_ONE_OF: {
        uint64_t count = 0;
        quux_count(context, argument, &count);
        for (uint64_t i = 0; run->raised && i < count; i++) {
            result->held = result->held || quux_vector_element(argument, i) == outcome_kind(run);
        }
        observe(context, result, run->raised ? outcome_kind(run) : run->outcome);
        break;
    }
    case ASSERT_SUPPRESSED_INCLUDES:
        result->held = run->raised && suppressed_includes(context, run->outcome, argument);
        observe(context, result,
                run->raised ? quux_error_field(run->outcome, QUUX_ERROR_SUPPRESSED) : run->outcome);
        break;
    case ASSERT_FUTURE_STATE:
    case ASSERT_FUTURE_ERROR:
        check_future_tag(context, run, kind, argument, assertion_argument(assertion, 1), result);
        break;
    case ASSERT_ELAPSED_UNDER:
    case ASSERT_ELAPSED_AT_LEAST: {
        uint64_t bound = (uint64_t)quux_integer_value(argument);
        result->held = kind == ASSERT_ELAPSED_UNDER ? run->elapsed_milliseconds < bound
                                                    : run->elapsed_milliseconds >= bound;
        result->observed = quux_small_integer((int64_t)run->elapsed_milliseconds);
        snprintf(result->observed_text, sizeof result->observed_text, "%llu ms",
                 (unsigned long long)run->elapsed_milliseconds);
        break;
    }
    case ASSERT_SAME_AS_SEQUENTIAL:
        result->held = run->raised == run->sequential_raised &&
                       quux_equal(context, run->raised ? outcome_kind(run) : run->outcome,
                                  run->sequential_raised
                                      ? quux_error_field(run->sequential_outcome, QUUX_ERROR_KIND)
                                      : run->sequential_outcome);
        observe(context, result, quux_boolean(result->held));
        break;
    default:
        break;
    }
    QUUX_ROOT_FRAME_END(context);
}

/* ---- Report (contracts/harness-report.md) ---- */

static bool report_entry(quux_context *context, program_run *run, quux_value assertion,
                         const verdict *result) {
    quux_value entry = quux_nothing;
    quux_value observed = result->observed;
    QUUX_ROOT_FRAME(context, &assertion, &entry, &observed);
    bool done =
        quux_map_empty(context, &entry) &&
        quux_map_put(context, entry, quux_keyword(context, "assertion"), assertion, &entry) &&
        quux_map_put(context, entry, quux_keyword(context, "held"), quux_boolean(result->held),
                     &entry) &&
        quux_map_put(context, entry, quux_keyword(context, "observed"), observed, &entry) &&
        quux_vector_add(context, run->report_entries, entry, &run->report_entries);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool put_keyword(quux_context *context, quux_value *map, const char *key,
                        const char *value) {
    return quux_map_put(context, *map, quux_keyword(context, key), quux_keyword(context, value),
                        map);
}

static void write_data_report(quux_context *context, program_run *run, const char *result_name) {
    quux_value report = quux_nothing;
    quux_value part = quux_nothing;
    quux_value text = quux_nothing;
    QUUX_ROOT_FRAME(context, &report, &part, &text);
    char seed[32];
    snprintf(seed, sizeof seed, "%016llx", (unsigned long long)run->seed);
    bool done =
        quux_map_empty(context, &report) && quux_make_string_from_c(context, run->name, &text) &&
        quux_map_put(context, report, quux_keyword(context, "program"), text, &report) &&
        quux_make_string_from_c(context, seed, &text) &&
        quux_map_put(context, report, quux_keyword(context, "seed"), text, &report) &&
        quux_map_empty(context, &part) &&
        put_keyword(context, &part, "system", QUUX_TARGET_SYSTEM) &&
        put_keyword(context, &part, "architecture", QUUX_TARGET_ARCHITECTURE) &&
        put_keyword(context, &part, "preset", QUUX_PRESET_NAME) &&
        quux_map_put(context, report, quux_keyword(context, "target"), part, &report) &&
        quux_map_put(context, report, quux_keyword(context, "assertions"), run->report_entries,
                     &report) &&
        put_keyword(context, &report, "result", result_name) && quux_map_empty(context, &part) &&
        quux_map_put(context, part, quux_keyword(context, "primary-error-raced"),
                     quux_boolean(quux_concurrency_primary_error_raced(context->runtime)), &part) &&
        quux_map_put(context, part, quux_keyword(context, "foreign-stream-advances"),
                     quux_small_integer(0), &part) &&
        quux_map_put(context, part, quux_keyword(context, "fetches"), quux_small_integer(0),
                     &part) &&
        quux_map_put(context, report, quux_keyword(context, "notes"), part, &report);
    FILE *file = done ? fopen(run->options->report_path, "w") : NULL;
    if (file == NULL) {
        fprintf(stderr, "quux-test: cannot write the report %s\n", run->options->report_path);
        context->pending_error = quux_nothing;
    } else {
        quux_print_to_stream(context, file, report, QUUX_PRINT_READABLE);
        fputs("\n", file);
        fclose(file);
    }
    QUUX_ROOT_FRAME_END(context);
}

static void print_report(quux_context *context, program_run *run) {
    printf("program %s   seed %016llx   target %s %s\n", run->name, (unsigned long long)run->seed,
           QUUX_TARGET_NAME, QUUX_PRESET_NAME);
    uint64_t count = 0;
    quux_count(context, run->assertions, &count);
    uint64_t held = 0;
    quux_vector_empty(context, &run->report_entries);
    for (uint64_t i = 0; i < count; i++) {
        quux_value assertion = quux_vector_element(run->assertions, i);
        verdict result;
        evaluate_assertion(context, run, assertion, &result);
        held += result.held ? 1 : 0;
        quux_text printed;
        quux_text_initialize(&printed);
        quux_print(context, &printed, assertion, QUUX_PRINT_READABLE);
        printf("  %-7s %-40s observed %s\n", result.held ? "held" : "FAILED",
               printed.bytes != NULL ? printed.bytes : "", result.observed_text);
        quux_text_release(&printed);
        report_entry(context, run, assertion, &result);
    }
    bool pass = held == count;
    printf("  result  %s (%llu/%llu)\n", pass ? "pass" : "FAILED", (unsigned long long)held,
           (unsigned long long)count);
    printf("  notes   primary-error-raced %s; foreign-stream-advances 0; fetches 0\n",
           quux_concurrency_primary_error_raced(context->runtime) ? "true" : "false");
    run->status = pass ? STATUS_PASS : STATUS_FAIL;
    if (run->options->report_path != NULL) {
        write_data_report(context, run, pass ? "pass" : "fail");
    }
}

/* ---- The run ---- */

static bool read_data(quux_context *context, program_run *run, const char *file_name,
                      const char *text, size_t length, quux_value *result) {
    if (!quux_read_one(context, text, length, result)) {
        char detail[1024];
        quux_value message = quux_error_field(context->pending_error, QUUX_ERROR_MESSAGE);
        snprintf(detail, sizeof detail, "%s does not read: %.*s", file_name,
                 quux_is_kind(message, QUUX_KIND_STRING) ? (int)quux_string_length(message) : 0,
                 quux_is_kind(message, QUUX_KIND_STRING) ? quux_string_bytes(message) : "");
        invalid(run, "%s", detail);
        context->pending_error = quux_nothing;
        return false;
    }
    return true;
}

static int run_program(quux_context *context, void *data) {
    program_run *run = data;
    quux_heap *heap = context->runtime->heap;
    quux_value *roots[] = {&run->conditions,         &run->assertions, &run->delays,
                           &run->failures,           &run->input,      &run->outcome,
                           &run->sequential_outcome, &run->tag_events, &run->report_entries};
    for (size_t i = 0; i < sizeof roots / sizeof roots[0]; i++) {
        *roots[i] = quux_nothing;
        quux_memory_register_global_root(heap, roots[i]);
    }
    quux_concurrency_set_seed(context->runtime, run->seed);
    if (!read_data(context, run, "conditions.qx", run->conditions_text, run->conditions_length,
                   &run->conditions) ||
        !read_data(context, run, "expected.qx", run->expected_text, run->expected_length,
                   &run->assertions) ||
        !validate_conditions(context, run) || !validate_assertions(context, run)) {
        return STATUS_INVALID;
    }
    /* the harness's tag replaces the plain one for this runtime */
    if (!quux_make_primitive(context, "tag", primitive_harness_tag, 2, 2,
                             &context->runtime->form_functions[QUUX_FORM_TAG])) {
        return STATUS_INVALID;
    }
    quux_value ignored = quux_nothing;
    QUUX_ROOT_FRAME(context, &ignored);
    quux_memory_set_verify_mode(context->runtime->heap, run->options->verify_mode);
    if (!quux_evaluate_text(context, run->main_text, run->main_length, &ignored)) {
        quux_memory_set_verify_mode(context->runtime->heap, QUUX_VERIFY_OFF);
        printf("program %s\n  main.qx does not load: ", run->name);
        quux_session_print_error(context, stdout, context->pending_error);
        context->pending_error = quux_nothing;
        QUUX_ROOT_FRAME_END(context);
        return STATUS_FAIL;
    }
    quux_thread *watchdog = NULL;
    if (!quux_thread_start(&watchdog, 1024u * 1024u, watch, run)) {
        fprintf(stderr, "quux-test: cannot start the watchdog\n");
        QUUX_ROOT_FRAME_END(context);
        return STATUS_INVALID;
    }
    uint64_t start = quux_monotonic_nanoseconds();
    run->raised = !run_entry(context, "main", &run->outcome);
    run->elapsed_milliseconds = (quux_monotonic_nanoseconds() - start) / 1000000u;
    if (run->raised) {
        run->outcome = context->pending_error;
        context->pending_error = quux_nothing;
    }
    if (run->needs_sequential) {
        run->sequential_raised = !run_entry(context, "sequential-main", &run->sequential_outcome);
        if (run->sequential_raised) {
            run->sequential_outcome = context->pending_error;
            context->pending_error = quux_nothing;
        }
    }
    stop_watchdog(run, watchdog);
    quux_memory_set_verify_mode(context->runtime->heap, QUUX_VERIFY_OFF);
    print_report(context, run);
    QUUX_ROOT_FRAME_END(context);
    return run->status;
}

static char *read_program_file(const char *directory, const char *file_name, size_t *length) {
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", directory, file_name);
    return quux_session_read_file(path, length);
}

int quux_run_program(const char *directory, const quux_program_options *options) {
    program_run run;
    memset(&run, 0, sizeof run);
    run.directory = directory;
    run.options = options;
    const char *slash = strrchr(directory, '/');
    run.name = slash != NULL && slash[1] != '\0' ? slash + 1 : directory;
    run.bound_milliseconds = 60000;
    run.status = STATUS_INVALID;
    if (options->seeded) {
        run.seed = options->seed;
    } else if (!quux_random_bytes(&run.seed, sizeof run.seed)) {
        run.seed = quux_monotonic_nanoseconds();
    }
    run.main_text = read_program_file(directory, "main.qx", &run.main_length);
    run.conditions_text = read_program_file(directory, "conditions.qx", &run.conditions_length);
    run.expected_text = read_program_file(directory, "expected.qx", &run.expected_length);
    int status = STATUS_INVALID;
    if (run.main_text == NULL || run.conditions_text == NULL || run.expected_text == NULL) {
        printf("program %s\n  invalid: main.qx, conditions.qx and expected.qx are required\n",
               run.name);
    } else {
        quux_mutex_initialize(&run.tag_mutex);
        quux_mutex_initialize(&run.watchdog_mutex);
        quux_condition_initialize(&run.watchdog_condition);
        current_run = &run;
        status = quux_session_run(run_program, &run);
        current_run = NULL;
        quux_condition_destroy(&run.watchdog_condition);
        quux_mutex_destroy(&run.watchdog_mutex);
        quux_mutex_destroy(&run.tag_mutex);
    }
    free(run.main_text);
    free(run.conditions_text);
    free(run.expected_text);
    fflush(stdout);
    return status;
}
