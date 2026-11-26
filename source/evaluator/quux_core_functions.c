/* The sequential core functions (sections 01, 02, 05, 12, 13, 14; T031). Each receives its
 * arguments in rooted memory and roots what it allocates. */
#include "quux_collections.h"
#include "quux_encoding.h"
#include "quux_evaluator.h"
#include "quux_printer.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PRIMITIVE(name)                                                                            \
    static bool name(quux_context *context, quux_value *arguments, uint32_t count,                 \
                     quux_value *result)

static bool expect_kind(quux_context *context, const char *function, quux_value value,
                        quux_kind kind) {
    if (quux_kind_of(value) == kind) {
        return true;
    }
    return quux_raise(context, "wrong-type", "%s expects %s, got %s", function,
                      quux_kind_phrase(kind), quux_kind_phrase(quux_kind_of(value)));
}

static bool is_number(quux_value value) {
    quux_kind kind = quux_kind_of(value);
    return kind == QUUX_KIND_INTEGER || kind == QUUX_KIND_FLOAT;
}

static bool expect_number(quux_context *context, const char *function, quux_value value) {
    if (is_number(value)) {
        return true;
    }
    return quux_raise(context, "wrong-type", "%s expects a number, got %s", function,
                      quux_kind_phrase(quux_kind_of(value)));
}

static double as_double(quux_value value) {
    return quux_kind_of(value) == QUUX_KIND_INTEGER ? (double)quux_integer_value(value)
                                                    : quux_float_value(value);
}

/* ---- Equality and identity (section 01) ---- */

PRIMITIVE(primitive_equal) {
    for (uint32_t i = 1; i < count; i++) {
        if (!quux_equal(context, arguments[0], arguments[i])) {
            *result = quux_false;
            return true;
        }
    }
    *result = quux_true;
    return true;
}

PRIMITIVE(primitive_not_equal) {
    (void)count;
    *result = quux_boolean(!quux_equal(context, arguments[0], arguments[1]));
    return true;
}

PRIMITIVE(primitive_hash) {
    (void)count;
    uint64_t hash = quux_value_hash(context, arguments[0]);
    return quux_make_integer(context, (int64_t)(hash >> 1), result);
}

PRIMITIVE(primitive_same_identity) {
    (void)context;
    (void)count;
    *result = quux_boolean(arguments[0] == arguments[1]);
    return true;
}

PRIMITIVE(primitive_not) {
    (void)context;
    (void)count;
    *result = quux_boolean(!quux_is_truthy(arguments[0]));
    return true;
}

/* ---- Kind predicates (docs/naming.md J3, D-50) ---- */

#define KIND_PREDICATE(function_name, condition)                                                   \
    PRIMITIVE(function_name) {                                                                     \
        (void)context;                                                                             \
        (void)count;                                                                               \
        quux_kind kind = quux_kind_of(arguments[0]);                                               \
        *result = quux_boolean(condition);                                                         \
        return true;                                                                               \
    }

KIND_PREDICATE(primitive_is_nothing, kind == QUUX_KIND_NOTHING)
KIND_PREDICATE(primitive_is_list, kind == QUUX_KIND_PAIR || kind == QUUX_KIND_NOTHING)
KIND_PREDICATE(primitive_is_vector, kind == QUUX_KIND_VECTOR)
KIND_PREDICATE(primitive_is_map, kind == QUUX_KIND_MAP)
KIND_PREDICATE(primitive_is_set, kind == QUUX_KIND_SET)
KIND_PREDICATE(primitive_is_keyword, kind == QUUX_KIND_KEYWORD)
KIND_PREDICATE(primitive_is_symbol, kind == QUUX_KIND_SYMBOL)
KIND_PREDICATE(primitive_is_string, kind == QUUX_KIND_STRING)
KIND_PREDICATE(primitive_is_integer, kind == QUUX_KIND_INTEGER)
KIND_PREDICATE(primitive_is_float, kind == QUUX_KIND_FLOAT)
KIND_PREDICATE(primitive_is_boolean, kind == QUUX_KIND_BOOLEAN)
KIND_PREDICATE(primitive_is_function, kind == QUUX_KIND_CLOSURE || kind == QUUX_KIND_PRIMITIVE)
KIND_PREDICATE(primitive_is_future, kind == QUUX_KIND_FUTURE)
KIND_PREDICATE(primitive_is_agent, kind == QUUX_KIND_AGENT)
KIND_PREDICATE(primitive_is_scope, kind == QUUX_KIND_SCOPE)
KIND_PREDICATE(primitive_is_stream, kind == QUUX_KIND_STREAM)
KIND_PREDICATE(primitive_is_error, kind == QUUX_KIND_ERROR)

/* ---- Numbers (section 05) ---- */

typedef enum arithmetic {
    ARITHMETIC_ADD,
    ARITHMETIC_SUBTRACT,
    ARITHMETIC_MULTIPLY,
    ARITHMETIC_DIVIDE,
} arithmetic;

static bool combine_numbers(quux_context *context, arithmetic operation, quux_value left,
                            quux_value right, quux_value *result) {
    if (quux_kind_of(left) == QUUX_KIND_INTEGER && quux_kind_of(right) == QUUX_KIND_INTEGER) {
        int64_t a = quux_integer_value(left);
        int64_t b = quux_integer_value(right);
        int64_t value = 0;
        bool overflowed = false;
        switch (operation) {
        case ARITHMETIC_ADD:
            overflowed = quux_add_overflows(a, b, &value);
            break;
        case ARITHMETIC_SUBTRACT:
            overflowed = quux_subtract_overflows(a, b, &value);
            break;
        case ARITHMETIC_MULTIPLY:
            overflowed = quux_multiply_overflows(a, b, &value);
            break;
        case ARITHMETIC_DIVIDE:
            if (b == 0) {
                return quux_raise(context, "division-by-zero", "%lld divided by zero",
                                  (long long)a);
            }
            if (a == INT64_MIN && b == -1) {
                overflowed = true;
                break;
            }
            if (a % b != 0) {
                return quux_raise(context, "inexact-division", "%lld is not divisible by %lld",
                                  (long long)a, (long long)b);
            }
            value = a / b;
            break;
        }
        if (overflowed) {
            return quux_raise(context, "overflow", "integer result does not fit in 64 bits");
        }
        return quux_make_integer(context, value, result);
    }
    double a = as_double(left);
    double b = as_double(right);
    double value = operation == ARITHMETIC_ADD        ? a + b
                   : operation == ARITHMETIC_SUBTRACT ? a - b
                   : operation == ARITHMETIC_MULTIPLY ? a * b
                                                      : a / b;
    return quux_make_float(context, value, result);
}

static bool fold_arithmetic(quux_context *context, const char *name, arithmetic operation,
                            quux_value identity, quux_value *arguments, uint32_t count,
                            quux_value *result) {
    for (uint32_t i = 0; i < count; i++) {
        if (!expect_number(context, name, arguments[i])) {
            return false;
        }
    }
    quux_value accumulated = identity;
    QUUX_ROOT_FRAME(context, &accumulated);
    bool done = true;
    uint32_t start = 0;
    if (count == 0) {
        accumulated = identity;
    } else if (count == 1 && (operation == ARITHMETIC_SUBTRACT || operation == ARITHMETIC_DIVIDE)) {
        done = combine_numbers(context, operation, identity, arguments[0], &accumulated);
        start = 1;
    } else {
        accumulated = arguments[0];
        start = 1;
    }
    for (uint32_t i = start; i < count && done; i++) {
        done = combine_numbers(context, operation, accumulated, arguments[i], &accumulated);
    }
    if (done) {
        *result = accumulated;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

PRIMITIVE(primitive_add) {
    return fold_arithmetic(context, "+", ARITHMETIC_ADD, quux_small_integer(0), arguments, count,
                           result);
}
PRIMITIVE(primitive_subtract) {
    return fold_arithmetic(context, "-", ARITHMETIC_SUBTRACT, quux_small_integer(0), arguments,
                           count, result);
}
PRIMITIVE(primitive_multiply) {
    return fold_arithmetic(context, "*", ARITHMETIC_MULTIPLY, quux_small_integer(1), arguments,
                           count, result);
}
PRIMITIVE(primitive_divide) {
    return fold_arithmetic(context, "/", ARITHMETIC_DIVIDE, quux_small_integer(1), arguments, count,
                           result);
}

typedef enum integer_division {
    DIVISION_QUOTIENT,
    DIVISION_REMAINDER,
    DIVISION_MODULO
} integer_division;

static bool divide_integers(quux_context *context, const char *name, integer_division kind,
                            quux_value *arguments, quux_value *result) {
    if (!expect_kind(context, name, arguments[0], QUUX_KIND_INTEGER) ||
        !expect_kind(context, name, arguments[1], QUUX_KIND_INTEGER)) {
        return false;
    }
    int64_t a = quux_integer_value(arguments[0]);
    int64_t b = quux_integer_value(arguments[1]);
    if (b == 0) {
        return quux_raise(context, "division-by-zero", "%s of %lld by zero", name, (long long)a);
    }
    if (a == INT64_MIN && b == -1) {
        if (kind == DIVISION_QUOTIENT) {
            return quux_raise(context, "overflow", "integer result does not fit in 64 bits");
        }
        return quux_make_integer(context, 0, result);
    }
    int64_t value;
    switch (kind) {
    case DIVISION_QUOTIENT:
        value = a / b;
        break;
    case DIVISION_REMAINDER:
        value = a % b;
        break;
    default:
        value = a % b;
        if (value != 0 && ((value < 0) != (b < 0))) {
            value += b;
        }
        break;
    }
    return quux_make_integer(context, value, result);
}

PRIMITIVE(primitive_quotient) {
    (void)count;
    return divide_integers(context, "quotient", DIVISION_QUOTIENT, arguments, result);
}
PRIMITIVE(primitive_remainder) {
    (void)count;
    return divide_integers(context, "remainder", DIVISION_REMAINDER, arguments, result);
}
PRIMITIVE(primitive_modulo) {
    (void)count;
    return divide_integers(context, "modulo", DIVISION_MODULO, arguments, result);
}

PRIMITIVE(primitive_increment) {
    (void)count;
    if (!expect_number(context, "increment", arguments[0])) {
        return false;
    }
    return combine_numbers(context, ARITHMETIC_ADD, arguments[0], quux_small_integer(1), result);
}

PRIMITIVE(primitive_decrement) {
    (void)count;
    if (!expect_number(context, "decrement", arguments[0])) {
        return false;
    }
    return combine_numbers(context, ARITHMETIC_SUBTRACT, arguments[0], quux_small_integer(1),
                           result);
}

PRIMITIVE(primitive_absolute_value) {
    (void)count;
    if (!expect_number(context, "absolute-value", arguments[0])) {
        return false;
    }
    if (quux_kind_of(arguments[0]) == QUUX_KIND_FLOAT) {
        return quux_make_float(context, fabs(quux_float_value(arguments[0])), result);
    }
    int64_t value = quux_integer_value(arguments[0]);
    if (value == INT64_MIN) {
        return quux_raise(context, "overflow", "integer result does not fit in 64 bits");
    }
    return quux_make_integer(context, value < 0 ? -value : value, result);
}

/* -1, 0, 1, or 2 when unordered (a NaN is involved). */
static int compare_numbers(quux_value left, quux_value right) {
    if (quux_kind_of(left) == QUUX_KIND_INTEGER && quux_kind_of(right) == QUUX_KIND_INTEGER) {
        int64_t a = quux_integer_value(left);
        int64_t b = quux_integer_value(right);
        return a < b ? -1 : a > b ? 1 : 0;
    }
    double a = as_double(left);
    double b = as_double(right);
    if (isnan(a) || isnan(b)) {
        return 2;
    }
    return a < b ? -1 : a > b ? 1 : 0;
}

typedef enum comparison {
    COMPARE_LESS,
    COMPARE_GREATER,
    COMPARE_LESS_OR_EQUAL,
    COMPARE_GREATER_OR_EQUAL,
    COMPARE_NUMERICALLY_EQUAL
} comparison;

static bool compare_chain(quux_context *context, const char *name, comparison kind,
                          quux_value *arguments, uint32_t count, quux_value *result) {
    for (uint32_t i = 0; i < count; i++) {
        if (!expect_number(context, name, arguments[i])) {
            return false;
        }
    }
    bool holds = true;
    for (uint32_t i = 1; i < count && holds; i++) {
        int order = compare_numbers(arguments[i - 1], arguments[i]);
        switch (kind) {
        case COMPARE_LESS:
            holds = order == -1;
            break;
        case COMPARE_GREATER:
            holds = order == 1;
            break;
        case COMPARE_LESS_OR_EQUAL:
            holds = order == -1 || order == 0;
            break;
        case COMPARE_GREATER_OR_EQUAL:
            holds = order == 1 || order == 0;
            break;
        default:
            holds = order == 0;
            break;
        }
    }
    *result = quux_boolean(holds);
    return true;
}

PRIMITIVE(primitive_less) {
    return compare_chain(context, "<", COMPARE_LESS, arguments, count, result);
}
PRIMITIVE(primitive_greater) {
    return compare_chain(context, ">", COMPARE_GREATER, arguments, count, result);
}
PRIMITIVE(primitive_less_or_equal) {
    return compare_chain(context, "<=", COMPARE_LESS_OR_EQUAL, arguments, count, result);
}
PRIMITIVE(primitive_greater_or_equal) {
    return compare_chain(context, ">=", COMPARE_GREATER_OR_EQUAL, arguments, count, result);
}
PRIMITIVE(primitive_numerically_equal) {
    return compare_chain(context, "numerically-equal?", COMPARE_NUMERICALLY_EQUAL, arguments, count,
                         result);
}

static bool extreme(quux_context *context, const char *name, bool maximum, quux_value *arguments,
                    uint32_t count, quux_value *result) {
    for (uint32_t i = 0; i < count; i++) {
        if (!expect_number(context, name, arguments[i])) {
            return false;
        }
    }
    quux_value best = arguments[0];
    for (uint32_t i = 1; i < count; i++) {
        int order = compare_numbers(arguments[i], best);
        if (order == 2) {
            best = quux_kind_of(arguments[i]) == QUUX_KIND_FLOAT ? arguments[i] : best;
        } else if ((maximum && order == 1) || (!maximum && order == -1)) {
            best = arguments[i];
        }
    }
    *result = best;
    return true;
}

PRIMITIVE(primitive_minimum) {
    return extreme(context, "minimum", false, arguments, count, result);
}
PRIMITIVE(primitive_maximum) {
    return extreme(context, "maximum", true, arguments, count, result);
}

static bool sign_test(quux_context *context, const char *name, quux_value value, int wanted,
                      quux_value *result) {
    if (!expect_number(context, name, value)) {
        return false;
    }
    int order = compare_numbers(value, quux_small_integer(0));
    *result = quux_boolean(order == wanted);
    return true;
}

PRIMITIVE(primitive_is_zero) {
    (void)count;
    return sign_test(context, "zero?", arguments[0], 0, result);
}
PRIMITIVE(primitive_is_positive) {
    (void)count;
    return sign_test(context, "positive?", arguments[0], 1, result);
}
PRIMITIVE(primitive_is_negative) {
    (void)count;
    return sign_test(context, "negative?", arguments[0], -1, result);
}

PRIMITIVE(primitive_is_even) {
    (void)count;
    if (!expect_kind(context, "even?", arguments[0], QUUX_KIND_INTEGER)) {
        return false;
    }
    *result = quux_boolean(quux_integer_value(arguments[0]) % 2 == 0);
    return true;
}

PRIMITIVE(primitive_is_odd) {
    (void)count;
    if (!expect_kind(context, "odd?", arguments[0], QUUX_KIND_INTEGER)) {
        return false;
    }
    *result = quux_boolean(quux_integer_value(arguments[0]) % 2 != 0);
    return true;
}

/* ---- Collections (section 02) ---- */

PRIMITIVE(primitive_list) {
    quux_value list = quux_nothing;
    QUUX_ROOT_FRAME(context, &list);
    bool done = true;
    for (uint32_t i = count; i > 0 && done; i--) {
        done = quux_list_prepend(context, arguments[i - 1], list, &list);
    }
    if (done) {
        *result = list;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

PRIMITIVE(primitive_vector) {
    quux_value vector = quux_nothing;
    QUUX_ROOT_FRAME(context, &vector);
    bool done = quux_vector_empty(context, &vector);
    for (uint32_t i = 0; i < count && done; i++) {
        done = quux_vector_add(context, vector, arguments[i], &vector);
    }
    if (done) {
        *result = vector;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

PRIMITIVE(primitive_map_of) {
    if (count % 2 != 0) {
        return quux_raise(context, "wrong-type", "map-of needs keys and values in pairs");
    }
    quux_value map = quux_nothing;
    QUUX_ROOT_FRAME(context, &map);
    bool done = quux_map_empty(context, &map);
    for (uint32_t i = 0; i + 1 < count && done; i += 2) {
        done = quux_map_put(context, map, arguments[i], arguments[i + 1], &map);
    }
    if (done) {
        *result = map;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

PRIMITIVE(primitive_set_of) {
    quux_value set = quux_nothing;
    QUUX_ROOT_FRAME(context, &set);
    bool done = quux_set_empty(context, &set);
    for (uint32_t i = 0; i < count && done; i++) {
        done = quux_map_put(context, set, arguments[i], arguments[i], &set);
    }
    if (done) {
        *result = set;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

PRIMITIVE(primitive_prepend) {
    (void)count;
    return quux_list_prepend(context, arguments[0], arguments[1], result);
}

PRIMITIVE(primitive_add_element) {
    (void)count;
    return quux_add(context, arguments[0], arguments[1], result);
}

PRIMITIVE(primitive_put) {
    (void)count;
    quux_kind kind = quux_kind_of(arguments[0]);
    if (kind == QUUX_KIND_MAP) {
        return quux_map_put(context, arguments[0], arguments[1], arguments[2], result);
    }
    if (kind == QUUX_KIND_VECTOR) {
        if (!expect_kind(context, "put", arguments[1], QUUX_KIND_INTEGER)) {
            return false;
        }
        int64_t index = quux_integer_value(arguments[1]);
        if (index < 0) {
            return quux_raise(context, "index-out-of-range", "index %lld", (long long)index);
        }
        return quux_vector_put(context, arguments[0], (uint64_t)index, arguments[2], result);
    }
    return quux_raise(context, "wrong-type", "put expects a map or a vector, got %s",
                      quux_kind_phrase(kind));
}

PRIMITIVE(primitive_remove_key) {
    (void)count;
    quux_kind kind = quux_kind_of(arguments[0]);
    if (kind != QUUX_KIND_MAP && kind != QUUX_KIND_SET) {
        return quux_raise(context, "wrong-type", "remove-key expects a map or a set, got %s",
                          quux_kind_phrase(kind));
    }
    return quux_map_remove(context, arguments[0], arguments[1], result);
}

PRIMITIVE(primitive_get) {
    return quux_get(context, arguments[0], arguments[1], count > 2 ? arguments[2] : quux_nothing,
                    result);
}

PRIMITIVE(primitive_has_key) {
    (void)count;
    quux_kind kind = quux_kind_of(arguments[0]);
    if (kind == QUUX_KIND_MAP || kind == QUUX_KIND_SET) {
        quux_value found;
        *result = quux_boolean(quux_map_lookup(context, arguments[0], arguments[1], &found));
        return true;
    }
    if (kind == QUUX_KIND_VECTOR) {
        *result = quux_boolean(quux_kind_of(arguments[1]) == QUUX_KIND_INTEGER &&
                               quux_integer_value(arguments[1]) >= 0 &&
                               (uint64_t)quux_integer_value(arguments[1]) <
                                   quux_vector_count(arguments[0]));
        return true;
    }
    if (kind == QUUX_KIND_NOTHING) {
        *result = quux_false;
        return true;
    }
    return quux_raise(context, "wrong-type", "has-key? expects a map, set or vector, got %s",
                      quux_kind_phrase(kind));
}

PRIMITIVE(primitive_count) {
    (void)count;
    uint64_t size = 0;
    if (!quux_count(context, arguments[0], &size)) {
        return false;
    }
    return quux_make_integer(context, (int64_t)size, result);
}

PRIMITIVE(primitive_is_empty) {
    (void)count;
    uint64_t size = 0;
    if (!quux_count(context, arguments[0], &size)) {
        return false;
    }
    *result = quux_boolean(size == 0);
    return true;
}

PRIMITIVE(primitive_first) {
    (void)count;
    quux_value value = arguments[0];
    switch (quux_kind_of(value)) {
    case QUUX_KIND_NOTHING:
        *result = quux_nothing;
        return true;
    case QUUX_KIND_PAIR:
        *result = quux_pair_first(value);
        return true;
    case QUUX_KIND_VECTOR:
        *result = quux_vector_count(value) > 0 ? quux_vector_element(value, 0) : quux_nothing;
        return true;
    default: {
        quux_value list = quux_nothing;
        QUUX_ROOT_FRAME(context, &list);
        bool done = quux_as_list(context, value, &list);
        if (done) {
            *result = quux_is_nothing(list) ? quux_nothing : quux_pair_first(list);
        }
        QUUX_ROOT_FRAME_END(context);
        return done;
    }
    }
}

PRIMITIVE(primitive_rest) {
    (void)count;
    if (quux_kind_of(arguments[0]) == QUUX_KIND_PAIR) {
        *result = quux_pair_rest(arguments[0]);
        return true;
    }
    quux_value list = quux_nothing;
    QUUX_ROOT_FRAME(context, &list);
    bool done = quux_as_list(context, arguments[0], &list);
    if (done) {
        *result = quux_is_nothing(list) ? quux_nothing : quux_pair_rest(list);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

PRIMITIVE(primitive_as_sequence) {
    (void)count;
    return quux_as_list(context, arguments[0], result);
}

typedef enum selection { SELECT_TRANSFORM, SELECT_FILTER, SELECT_REJECT } selection;

static bool select_elements(quux_context *context, selection kind, quux_value function,
                            quux_value collection, quux_value *result) {
    quux_value list = quux_nothing;
    quux_value collected = quux_nothing;
    quux_value value = quux_nothing;
    QUUX_ROOT_FRAME(context, &function, &list, &collected, &value);
    bool done = quux_as_list(context, collection, &list);
    for (quux_value cell = list; done && !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
        quux_value element = quux_pair_first(cell);
        done = quux_call1(context, function, element, &value);
        if (!done) {
            break;
        }
        if (kind == SELECT_TRANSFORM) {
            done = quux_list_prepend(context, value, collected, &collected);
        } else if (quux_is_truthy(value) == (kind == SELECT_FILTER)) {
            done = quux_list_prepend(context, element, collected, &collected);
        }
    }
    done = done && quux_list_reverse(context, collected, result);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

PRIMITIVE(primitive_transform) {
    (void)count;
    return select_elements(context, SELECT_TRANSFORM, arguments[0], arguments[1], result);
}
PRIMITIVE(primitive_filter) {
    (void)count;
    return select_elements(context, SELECT_FILTER, arguments[0], arguments[1], result);
}
PRIMITIVE(primitive_reject) {
    (void)count;
    return select_elements(context, SELECT_REJECT, arguments[0], arguments[1], result);
}

PRIMITIVE(primitive_reduce) {
    (void)count;
    quux_value list = quux_nothing;
    quux_value accumulated = arguments[1];
    QUUX_ROOT_FRAME(context, &list, &accumulated);
    bool done = quux_as_list(context, arguments[2], &list);
    for (quux_value cell = list; done && !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
        done = quux_call2(context, arguments[0], accumulated, quux_pair_first(cell), &accumulated);
    }
    if (done) {
        *result = accumulated;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

PRIMITIVE(primitive_pour_into) {
    (void)count;
    quux_value list = quux_nothing;
    quux_value target = arguments[0];
    QUUX_ROOT_FRAME(context, &list, &target);
    bool done = quux_as_list(context, arguments[1], &list);
    for (quux_value cell = list; done && !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
        done = quux_add(context, target, quux_pair_first(cell), &target);
    }
    if (done) {
        *result = target;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

PRIMITIVE(primitive_range) {
    int64_t start = 0;
    int64_t end;
    if (count == 1) {
        if (!expect_kind(context, "range", arguments[0], QUUX_KIND_INTEGER)) {
            return false;
        }
        end = quux_integer_value(arguments[0]);
    } else {
        if (!expect_kind(context, "range", arguments[0], QUUX_KIND_INTEGER) ||
            !expect_kind(context, "range", arguments[1], QUUX_KIND_INTEGER)) {
            return false;
        }
        start = quux_integer_value(arguments[0]);
        end = quux_integer_value(arguments[1]);
    }
    quux_value list = quux_nothing;
    quux_value number = quux_nothing;
    QUUX_ROOT_FRAME(context, &list, &number);
    bool done = true;
    for (int64_t i = end; i > start && done; i--) {
        done = quux_make_integer(context, i - 1, &number) &&
               quux_list_prepend(context, number, list, &list);
    }
    if (done) {
        *result = list;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

PRIMITIVE(primitive_element_at) {
    (void)count;
    if (!expect_kind(context, "element-at", arguments[1], QUUX_KIND_INTEGER)) {
        return false;
    }
    int64_t index = quux_integer_value(arguments[1]);
    quux_kind kind = quux_kind_of(arguments[0]);
    uint64_t size = 0;
    if (kind != QUUX_KIND_VECTOR && kind != QUUX_KIND_PAIR && kind != QUUX_KIND_NOTHING) {
        return quux_raise(context, "wrong-type", "element-at expects a vector or a list, got %s",
                          quux_kind_phrase(kind));
    }
    if (!quux_count(context, arguments[0], &size)) {
        return false;
    }
    if (index < 0 || (uint64_t)index >= size) {
        return quux_raise(context, "index-out-of-range", "index %lld outside 0 ... %lld",
                          (long long)index, (long long)size - 1);
    }
    if (kind == QUUX_KIND_VECTOR) {
        *result = quux_vector_element(arguments[0], (uint64_t)index);
        return true;
    }
    quux_value cell = arguments[0];
    for (int64_t i = 0; i < index; i++) {
        cell = quux_pair_rest(cell);
    }
    *result = quux_pair_first(cell);
    return true;
}

PRIMITIVE(primitive_to_string) {
    quux_text text;
    quux_text_initialize(&text);
    for (uint32_t i = 0; i < count; i++) {
        quux_print(context, &text, arguments[i], QUUX_PRINT_DISPLAY);
    }
    bool done = !text.failed && quux_make_string(context, text.bytes == NULL ? "" : text.bytes,
                                                 text.length, result);
    if (text.failed) {
        done = quux_raise(context, "resource-exhausted", "out of memory in to-string");
    }
    quux_text_release(&text);
    return done;
}

PRIMITIVE(primitive_print_line) {
    (void)count;
    quux_text text;
    quux_text_initialize(&text);
    quux_print(context, &text, arguments[0], QUUX_PRINT_DISPLAY);
    quux_text_append_c(&text, "\n");
    if (text.failed) {
        quux_text_release(&text);
        return quux_raise(context, "resource-exhausted", "out of memory in print-line");
    }
    FILE *output = context->runtime->output;
    fwrite(text.bytes, 1, text.length, output);
    fflush(output);
    quux_text_release(&text);
    *result = quux_nothing;
    return true;
}

PRIMITIVE(primitive_parse_integer) {
    (void)count;
    if (!expect_kind(context, "parse-integer", arguments[0], QUUX_KIND_STRING)) {
        return false;
    }
    const char *text = quux_string_bytes(arguments[0]);
    size_t length = quux_string_length(arguments[0]);
    size_t start = (length > 0 && (text[0] == '-' || text[0] == '+')) ? 1 : 0;
    bool valid = length > start && length < 64;
    for (size_t i = start; i < length && valid; i++) {
        valid = text[i] >= '0' && text[i] <= '9';
    }
    if (!valid) {
        return quux_raise(context, "wrong-type", "\"%.*s\" is not a decimal integer",
                          (int)(length > 64 ? 64 : length), text);
    }
    errno = 0;
    long long number = strtoll(text, NULL, 10);
    if (errno == ERANGE) {
        return quux_raise(context, "overflow", "\"%s\" does not fit in 64 bits", text);
    }
    return quux_make_integer(context, (int64_t)number, result);
}

/* ---- Errors (section 12) ---- */

PRIMITIVE(primitive_error) {
    if (!expect_kind(context, "error", arguments[0], QUUX_KIND_KEYWORD) ||
        !expect_kind(context, "error", arguments[1], QUUX_KIND_STRING)) {
        return false;
    }
    quux_value data = count > 2 ? arguments[2] : quux_nothing;
    QUUX_ROOT_FRAME(context, &data);
    bool done = true;
    if (quux_is_nothing(data)) {
        done = quux_map_empty(context, &data);
    }
    done = done && quux_make_error(context, arguments[0], arguments[1], data, result);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

PRIMITIVE(primitive_error_kind) {
    (void)count;
    if (!expect_kind(context, "error-kind", arguments[0], QUUX_KIND_ERROR)) {
        return false;
    }
    *result = quux_error_field(arguments[0], QUUX_ERROR_KIND);
    return true;
}

PRIMITIVE(primitive_error_data) {
    (void)count;
    if (!expect_kind(context, "error-data", arguments[0], QUUX_KIND_ERROR)) {
        return false;
    }
    quux_value data = quux_error_field(arguments[0], QUUX_ERROR_DATA);
    if (quux_is_nothing(data)) {
        return quux_map_empty(context, result);
    }
    *result = data;
    return true;
}

/* ---- Macros and identity (sections 04, 13) ---- */

PRIMITIVE(primitive_unique_symbol) {
    if (count == 0) {
        return quux_make_unique_symbol(context, "symbol", 6, result);
    }
    quux_kind kind = quux_kind_of(arguments[0]);
    if (kind == QUUX_KIND_STRING) {
        return quux_make_unique_symbol(context, quux_string_bytes(arguments[0]),
                                       quux_string_length(arguments[0]), result);
    }
    if (kind == QUUX_KIND_SYMBOL || kind == QUUX_KIND_KEYWORD) {
        return quux_make_unique_symbol(context, quux_name_bytes(arguments[0]),
                                       quux_name_length(arguments[0]), result);
    }
    return quux_raise(context, "wrong-type", "unique-symbol expects a string prefix");
}

PRIMITIVE(primitive_function_hash) {
    (void)count;
    quux_kind kind = quux_kind_of(arguments[0]);
    if (kind != QUUX_KIND_CLOSURE && kind != QUUX_KIND_PRIMITIVE) {
        return quux_raise(context, "wrong-type", "function-hash expects a function, got %s",
                          quux_kind_phrase(kind));
    }
    unsigned char digest[QUUX_CONTENT_HASH_BYTES];
    if (!quux_function_content_hash(context, arguments[0], digest)) {
        return false;
    }
    char hexadecimal[2 * QUUX_CONTENT_HASH_BYTES + 1];
    for (int i = 0; i < QUUX_CONTENT_HASH_BYTES; i++) {
        snprintf(hexadecimal + 2 * i, 3, "%02x", digest[i]);
    }
    return quux_make_string(context, hexadecimal, 2 * QUUX_CONTENT_HASH_BYTES, result);
}

/* ---- Registration ---- */

typedef struct primitive_entry {
    const char *name;
    quux_primitive_function function;
    uint32_t minimum;
    uint32_t maximum;
} primitive_entry;

static const primitive_entry core_primitives[] = {
    {"equal?", primitive_equal, 1, QUUX_VARIADIC},
    {"not-equal?", primitive_not_equal, 2, 2},
    {"numerically-equal?", primitive_numerically_equal, 1, QUUX_VARIADIC},
    {"hash", primitive_hash, 1, 1},
    {"same-identity?", primitive_same_identity, 2, 2},
    {"not", primitive_not, 1, 1},
    {"nothing?", primitive_is_nothing, 1, 1},
    {"list?", primitive_is_list, 1, 1},
    {"vector?", primitive_is_vector, 1, 1},
    {"map?", primitive_is_map, 1, 1},
    {"set?", primitive_is_set, 1, 1},
    {"keyword?", primitive_is_keyword, 1, 1},
    {"symbol?", primitive_is_symbol, 1, 1},
    {"string?", primitive_is_string, 1, 1},
    {"integer?", primitive_is_integer, 1, 1},
    {"floating-point?", primitive_is_float, 1, 1},
    {"boolean?", primitive_is_boolean, 1, 1},
    {"function?", primitive_is_function, 1, 1},
    {"future?", primitive_is_future, 1, 1},
    {"agent?", primitive_is_agent, 1, 1},
    {"scope?", primitive_is_scope, 1, 1},
    {"stream?", primitive_is_stream, 1, 1},
    {"error?", primitive_is_error, 1, 1},
    {"+", primitive_add, 0, QUUX_VARIADIC},
    {"-", primitive_subtract, 1, QUUX_VARIADIC},
    {"*", primitive_multiply, 0, QUUX_VARIADIC},
    {"/", primitive_divide, 1, QUUX_VARIADIC},
    {"quotient", primitive_quotient, 2, 2},
    {"remainder", primitive_remainder, 2, 2},
    {"modulo", primitive_modulo, 2, 2},
    {"increment", primitive_increment, 1, 1},
    {"decrement", primitive_decrement, 1, 1},
    {"absolute-value", primitive_absolute_value, 1, 1},
    {"minimum", primitive_minimum, 1, QUUX_VARIADIC},
    {"maximum", primitive_maximum, 1, QUUX_VARIADIC},
    {"zero?", primitive_is_zero, 1, 1},
    {"positive?", primitive_is_positive, 1, 1},
    {"negative?", primitive_is_negative, 1, 1},
    {"even?", primitive_is_even, 1, 1},
    {"odd?", primitive_is_odd, 1, 1},
    {"<", primitive_less, 1, QUUX_VARIADIC},
    {">", primitive_greater, 1, QUUX_VARIADIC},
    {"<=", primitive_less_or_equal, 1, QUUX_VARIADIC},
    {">=", primitive_greater_or_equal, 1, QUUX_VARIADIC},
    {"list", primitive_list, 0, QUUX_VARIADIC},
    {"vector", primitive_vector, 0, QUUX_VARIADIC},
    {"map-of", primitive_map_of, 0, QUUX_VARIADIC},
    {"set-of", primitive_set_of, 0, QUUX_VARIADIC},
    {"prepend", primitive_prepend, 2, 2},
    {"add", primitive_add_element, 2, 2},
    {"put", primitive_put, 3, 3},
    {"remove-key", primitive_remove_key, 2, 2},
    {"get", primitive_get, 2, 3},
    {"has-key?", primitive_has_key, 2, 2},
    {"count", primitive_count, 1, 1},
    {"empty?", primitive_is_empty, 1, 1},
    {"first", primitive_first, 1, 1},
    {"rest", primitive_rest, 1, 1},
    {"as-sequence", primitive_as_sequence, 1, 1},
    {"transform", primitive_transform, 2, 2},
    {"filter", primitive_filter, 2, 2},
    {"reject", primitive_reject, 2, 2},
    {"reduce", primitive_reduce, 3, 3},
    {"pour-into", primitive_pour_into, 2, 2},
    {"range", primitive_range, 1, 2},
    {"element-at", primitive_element_at, 2, 2},
    {"to-string", primitive_to_string, 0, QUUX_VARIADIC},
    {"print-line", primitive_print_line, 1, 1},
    {"parse-integer", primitive_parse_integer, 1, 1},
    {"error", primitive_error, 2, 3},
    {"error-kind", primitive_error_kind, 1, 1},
    {"error-data", primitive_error_data, 1, 1},
    {"unique-symbol", primitive_unique_symbol, 0, 1},
    {"function-hash", primitive_function_hash, 1, 1},
};

bool quux_core_functions_register(quux_context *context) {
    for (size_t i = 0; i < sizeof core_primitives / sizeof core_primitives[0]; i++) {
        const primitive_entry *entry = &core_primitives[i];
        if (!quux_define_primitive(context, entry->name, entry->function, entry->minimum,
                                   entry->maximum)) {
            return false;
        }
    }
    return true;
}
