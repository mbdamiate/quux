#include "quux_unit_evaluation.h"

static int check_expander(quux_context *context, void *data) {
    quux_unit_state *state = data;
    /* quasi-quote qualifies free symbols with their namespace; unquote inserts values */
    QUUX_CHECK(state,
               quux_unit_evaluates_to(context, "(quasi-quote (print-line (unquote (+ 1 2))))",
                                      "(core/print-line 3)"));
    QUUX_CHECK(state, quux_unit_evaluates_to(
                          context, "(quasi-quote (list (unquote-splicing [1 2]) local-name))",
                          "(core/list 1 2 local-name)"));
    QUUX_CHECK(state, quux_unit_evaluates_to(context, "(quasi-quote (if sequence let))",
                                             "(if sequence let)"));
    QUUX_CHECK(state, quux_unit_evaluates_to(context, "(unquote 1)", "!! :wrong-type"));
    /* unique-symbol never clashes */
    QUUX_CHECK(state, quux_unit_evaluates_to(context,
                                             "(equal? (unique-symbol \"t\") (unique-symbol \"t\"))",
                                             "false"));
    /* :rest and patterns */
    QUUX_CHECK(state, quux_unit_evaluates_to(context, "((function [a :rest more] [a more]) 1 2 3)",
                                             "[1 (2 3)]"));
    QUUX_CHECK(state,
               quux_unit_evaluates_to(context, "((function [a :rest more] more) 1)", "nothing"));
    QUUX_CHECK(state,
               quux_unit_evaluates_to(context, "(let [[a [b c] :rest d] [1 [2 3] 4 5]] [a b c d])",
                                      "[1 2 3 (4 5)]"));
    QUUX_CHECK(state,
               quux_unit_evaluates_to(
                   context, "(let [{a :a {b :b} :inner} {:a 1 :inner {:b 2}}] [a b])", "[1 2]"));
    QUUX_CHECK(state, quux_unit_evaluates_to(context, "(let [[x y] [1]] y)", "nothing"));
    QUUX_CHECK(state, quux_unit_evaluates_to(context, "(let [{x :x} [1]] x)", "!! :wrong-type"));
    QUUX_CHECK(state,
               quux_unit_evaluates_to(context, "((function [[a b]] (+ a b)) [20 22])", "42"));
    /* macros: defined, expanded completely, not usable as values */
    QUUX_CHECK(state, quux_unit_evaluates_to(
                          context,
                          "(define-macro swap-arguments [call] "
                          "  (list (first call) (first (rest (rest call))) (first (rest call))))"
                          "(swap-arguments (- 10 3))",
                          "-7"));
    QUUX_CHECK(state,
               quux_unit_evaluates_to(context, "(swap-arguments 1 2)", "!! :wrong-argument-count"));
    QUUX_CHECK(state, quux_unit_evaluates_to(context, "swap-arguments", "!! :wrong-type"));
    QUUX_CHECK(state, quux_unit_evaluates_to(context, "(case 2 1 :one 2 :two :other)", ":two"));
    QUUX_CHECK(state,
               quux_unit_evaluates_to(
                   context, "(thread-last [1 2 3] (transform increment) (reduce + 0))", "9"));
    return 0;
}

static void expander_behaviour(quux_unit_state *state) {
    QUUX_CHECK(state, quux_session_run(check_expander, state) == 0);
}

QUUX_UNIT_CASES(expander, {"quasi-quote, patterns, :rest and macros", expander_behaviour});
