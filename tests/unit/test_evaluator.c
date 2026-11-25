#include "quux_unit_evaluation.h"

static int check_tail_calls(quux_context *context, void *data) {
    quux_unit_state *state = data;
    /* one million self tail calls and one million mutual tail calls: if tail calls used the C
     * stack, the depth limit would raise :stack-exhausted long before */
    QUUX_CHECK(state,
               quux_unit_evaluates_to(context,
                                      "(define-function sum-to [n total] "
                                      "  (if (zero? n) total (sum-to (decrement n) (+ total n))))"
                                      "(sum-to 1000000 0)",
                                      "500000500000"));
    QUUX_CHECK(state, quux_unit_evaluates_to(
                          context,
                          "(let-functions [(ping [n] (if (zero? n) :ping (pong (decrement n))))"
                          "                (pong [n] (if (zero? n) :pong (ping (decrement n))))]"
                          "  (ping 1000001))",
                          ":pong"));
    QUUX_CHECK(state, quux_unit_evaluates_to(
                          context,
                          "(define-function deep [n] (if (zero? n) 0 (+ 1 (deep (decrement n)))))"
                          "(deep 1000000)",
                          "!! :stack-exhausted"));
    QUUX_CHECK(state, quux_unit_evaluates_to(context, "(deep 100)", "100"));
    return 0;
}

static int check_content_hashes(quux_context *context, void *data) {
    quux_unit_state *state = data;
    /* a mutually recursive group: renaming the members and reordering their definitions keeps
     * every hash (section 13 rule 3) */
    QUUX_CHECK(state, quux_unit_evaluates_to(
                          context,
                          "(namespace first-copy)"
                          "(define-function ping [n] (if (zero? n) :ping (pong (decrement n))))"
                          "(define-function pong [n] (if (zero? n) :pong (ping (decrement n))))"
                          "(define ping-hash (function-hash ping))"
                          "(define pong-hash (function-hash pong))"
                          "(namespace second-copy (:import first-copy))"
                          "(define-function respond [n] (if (zero? n) :pong (call (decrement n))))"
                          "(define-function call [n] (if (zero? n) :ping (respond (decrement n))))"
                          "[(equal? (function-hash call) first-copy/ping-hash)"
                          " (equal? (function-hash respond) first-copy/pong-hash)"
                          " (equal? (function-hash call) (function-hash respond))]",
                          "[true true false]"));
    /* closures: same code, different captures */
    QUUX_CHECK(state, quux_unit_evaluates_to(
                          context,
                          "(define-function adder [n] (function [x] (+ x n)))"
                          "[(equal? (function-hash (adder 1)) (function-hash (adder 2)))"
                          " (equal? (adder 1) (adder 2)) (equal? (adder 1) (adder 1))]",
                          "[true false true]"));
    return 0;
}

static int check_errors(quux_context *context, void *data) {
    quux_unit_state *state = data;
    QUUX_CHECK(state, quux_unit_evaluates_to(context, "(undefined-thing 1)", "!! :undefined-name"));
    QUUX_CHECK(state, quux_unit_evaluates_to(context, "(42 1)", "!! :wrong-type"));
    QUUX_CHECK(state, quux_unit_evaluates_to(
                          context,
                          "(try (raise (error :user \"no\" {:a 1})) "
                          "  (catch :overflow e :wrong) (catch :user e (get (error-data e) :a)))",
                          "1"));
    QUUX_CHECK(state, quux_unit_evaluates_to(context, "(try (quotient 1 0) (finally 1))",
                                             "!! :division-by-zero"));
    QUUX_CHECK(state, quux_unit_evaluates_to(context, "(raise 42)", "!! :wrong-type"));
    return 0;
}

/* An unqualified name is looked up in the namespace, then in its imports in the order they are
 * written, then in core (section 14; found by ergonomics program 08). */
static int check_import_order(quux_context *context, void *data) {
    quux_unit_state *state = data;
    QUUX_CHECK(state, quux_unit_evaluates_to(context,
                                             "(namespace first-source)"
                                             "(define-function label [] :first)"
                                             "(namespace second-source)"
                                             "(define-function label [] :second)"
                                             "(define-function only-second [] :only-second)"
                                             "(namespace both (:import first-source second-source))"
                                             "[(label) (only-second) (count [1 2])]",
                                             "[:first :only-second 2]"));
    QUUX_CHECK(state,
               quux_unit_evaluates_to(context,
                                      "(namespace reversed (:import second-source first-source))"
                                      "(label)",
                                      ":second"));
    return 0;
}

/* Root verification from the first allocation, including the prelude. */
static void *verify_everything(void *argument) {
    quux_unit_state *state = argument;
    quux_runtime *runtime = quux_runtime_create();
    quux_context context;
    quux_context_initialize(&context, runtime);
    quux_memory_set_verify_mode(runtime->heap, QUUX_VERIFY_EVERY_ALLOCATION);
    QUUX_CHECK(state, quux_evaluator_initialize(&context));
    QUUX_CHECK(state,
               quux_unit_evaluates_to(
                   &context,
                   "(define-function total [items] (reduce + 0 items))"
                   "(let [{values :values} {:values [1 2 3]}] (total (pour-into #{} values)))",
                   "6"));
    quux_memory_set_verify_mode(runtime->heap, QUUX_VERIFY_OFF);
    quux_context_finish(&context);
    quux_runtime_destroy(runtime);
    return NULL;
}

static void tail_calls_run_in_constant_stack(quux_unit_state *state) {
    QUUX_CHECK(state, quux_session_run(check_tail_calls, state) == 0);
}

static void content_hashes_ignore_names(quux_unit_state *state) {
    QUUX_CHECK(state, quux_session_run(check_content_hashes, state) == 0);
}

static void errors_are_raised_and_caught(quux_unit_state *state) {
    QUUX_CHECK(state, quux_session_run(check_errors, state) == 0);
}

static void imports_are_searched_in_order(quux_unit_state *state) {
    QUUX_CHECK(state, quux_session_run(check_import_order, state) == 0);
}

static void prelude_follows_the_root_convention(quux_unit_state *state) {
    quux_thread *thread = NULL;
    QUUX_CHECK(state, quux_thread_start(&thread, 512u * 1024u * 1024u, verify_everything, state));
    if (thread != NULL) {
        quux_thread_join(thread);
    }
}

QUUX_UNIT_CASES(evaluator, {"tail calls run in constant stack", tail_calls_run_in_constant_stack},
                {"content hashes ignore names and order", content_hashes_ignore_names},
                {"errors are raised and caught", errors_are_raised_and_caught},
                {"imports are searched in order", imports_are_searched_in_order},
                {"the prelude follows the root convention", prelude_follows_the_root_convention});
