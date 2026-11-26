/* Futures, scopes, cancellation and deadlock detection on the blocking executor (T038; sections
 * 06 and 07, D-40). */
#include "quux_unit_evaluation.h"

/* A future leaves :empty once: when completion and a timeout race, the first transition wins
 * and the state never changes afterwards (section 06 rule 5). */
static int check_single_transition(quux_context *context, void *data) {
    quux_unit_state *state = data;
    for (int round = 0; round < 20; round++) {
        /* an agent carries the future out of its scope, which fails when the timeout wins */
        QUUX_CHECK(
            state,
            quux_unit_evaluates_to(
                context,
                "(let [holder (agent nothing)"
                "      outcome (try"
                "                (with-scope [scope]"
                "                  (let [raced (with-timeout 2 (function [] (sleep 2) :done))]"
                "                    (wait-for (update holder (function [old] raced)))"
                "                    (wait-for raced)))"
                "                (catch :timeout problem :timeout))"
                "      raced (state holder)]"
                "  (and (equal? (state raced) (state raced))"
                "       (or (and (equal? outcome :done) (equal? (state raced) :filled))"
                "           (and (equal? outcome :timeout)"
                "                (equal? (state raced) :failed)))))",
                "true"));
    }
    return 0;
}

/* No future outlives its scope: one returned from it is already settled (section 06 rule 12). */
static int check_scope_join(quux_context *context, void *data) {
    quux_unit_state *state = data;
    QUUX_CHECK(state, quux_unit_evaluates_to(
                          context,
                          "(let [leaving (with-scope [scope] (future (sleep 20) :done))]"
                          "  [(state leaving) (wait-for leaving)])",
                          "[:filled :done]"));
    QUUX_CHECK(state, quux_unit_evaluates_to(
                          context,
                          "(with-scope [scope]"
                          "  (let [inner (with-scope [nested] (future (sleep 10) :inner))]"
                          "    (state inner)))",
                          ":filled"));
    return 0;
}

/* wait-for and sleep are cancellation points (section 07 rule 11). */
static int check_cancellation_points(quux_context *context, void *data) {
    quux_unit_state *state = data;
    QUUX_CHECK(state,
               quux_unit_evaluates_to(
                   context,
                   "(let [holder (agent nothing)"
                   "      outcome (try"
                   "                (with-scope [scope]"
                   "                  (let [slow (future (sleep 10000) :never)]"
                   "                    (wait-for (update holder (function [old] slow)))"
                   "                    (future (sleep 10) (cancel! scope))"
                   "                    (wait-for slow)))"
                   "                (catch :cancelled problem (error-kind problem)))"
                   "      slow (state holder)]"
                   "  [outcome (state slow)"
                   "   (try (wait-for slow) (catch :cancelled problem (error-kind problem)))])",
                   "[:cancelled :failed :cancelled]"));
    QUUX_CHECK(state, quux_unit_evaluates_to(context,
                                             "(try (with-scope [scope]"
                                             "       (cancel! scope)"
                                             "       (sleep 10000)"
                                             "       :not-cancelled)"
                                             "  (catch :cancelled problem :cancelled))",
                                             ":cancelled"));
    QUUX_CHECK(state, quux_unit_evaluates_to(context,
                                             "(try (with-scope [scope]"
                                             "       (cancel! scope)"
                                             "       (check-cancellation)"
                                             "       :not-cancelled)"
                                             "  (catch :cancelled problem :cancelled))",
                                             ":cancelled"));
    return 0;
}

/* shield-from-cancellation lets cleanup finish; the cancellation is observed afterwards
 * (section 07 rule 14), and finally runs when :cancelled is raised through it (rule 13). */
static int check_shield(quux_context *context, void *data) {
    quux_unit_state *state = data;
    QUUX_CHECK(state,
               quux_unit_evaluates_to(context,
                                      "(define journal (agent []))"
                                      "(try"
                                      "  (with-scope [scope]"
                                      "    (future"
                                      "      (try"
                                      "        (shield-from-cancellation"
                                      "          (sleep 50)"
                                      "          (wait-for (update journal add :cleanup)))"
                                      "        (sleep 10000)"
                                      "        (finally (wait-for (update journal add :finally)))))"
                                      "    (sleep 10)"
                                      "    (cancel! scope))"
                                      "  (catch :cancelled problem (state journal)))",
                                      "[:cleanup]"));
    return 0;
}

/* The first failure is primary; later ones are suppressed (section 07 rule 7, D-11). */
static int check_primary_and_suppressed(quux_context *context, void *data) {
    quux_unit_state *state = data;
    QUUX_CHECK(state, quux_unit_evaluates_to(
                          context,
                          "(try"
                          "  (with-scope [scope]"
                          "    (future (raise (error :user \"first\" {})))"
                          "    (future (raise (error :overflow \"second\" {})))"
                          "    :unreachable)"
                          "  (catch (function [candidate] true) caught"
                          "    [(count (get caught :suppressed))"
                          "     (equal? (set-of (error-kind caught)"
                          "                     (error-kind (first (get caught :suppressed))))"
                          "             #{:user :overflow})]))",
                          "[1 true]"));
    return 0;
}

/* Two tasks that wait for each other fail with :deadlock, and :cycle names both futures
 * (section 07 rule 16, D-14). */
static int check_deadlock(quux_context *context, void *data) {
    quux_unit_state *state = data;
    QUUX_CHECK(state, quux_unit_evaluates_to(
                          context,
                          "(try"
                          "  (with-scope [scope]"
                          "    (let [holder (agent {})"
                          "          a (future (sleep 30) (wait-for (get (state holder) :b)))"
                          "          b (future (sleep 30) (wait-for (get (state holder) :a)))]"
                          "      (wait-for (update holder put :a a))"
                          "      (wait-for (update holder put :b b))"
                          "      [(wait-for a) (wait-for b)]))"
                          "  (catch :deadlock problem"
                          "    (count (get (error-data problem) :cycle))))",
                          "2"));
    return 0;
}

static void single_transition(quux_unit_state *state) {
    QUUX_CHECK(state, quux_session_run(check_single_transition, state) == 0);
}

static void scope_join(quux_unit_state *state) {
    QUUX_CHECK(state, quux_session_run(check_scope_join, state) == 0);
}

static void cancellation_points(quux_unit_state *state) {
    QUUX_CHECK(state, quux_session_run(check_cancellation_points, state) == 0);
}

static void shield(quux_unit_state *state) {
    QUUX_CHECK(state, quux_session_run(check_shield, state) == 0);
}

static void primary_and_suppressed(quux_unit_state *state) {
    QUUX_CHECK(state, quux_session_run(check_primary_and_suppressed, state) == 0);
}

static void deadlock(quux_unit_state *state) {
    QUUX_CHECK(state, quux_session_run(check_deadlock, state) == 0);
}

QUUX_UNIT_CASES(concurrency, {"a future leaves :empty once", single_transition},
                {"a scope joins its futures", scope_join},
                {"wait-for and sleep are cancellation points", cancellation_points},
                {"shield-from-cancellation lets cleanup finish", shield},
                {"primary and suppressed errors", primary_and_suppressed},
                {"a two-task deadlock raises :deadlock", deadlock});
