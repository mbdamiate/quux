#include "quux_collections.h"
#include "quux_unit_test.h"

typedef struct fixture {
    quux_runtime *runtime;
    quux_context context;
} fixture;

static void open_fixture(fixture *fixture) {
    fixture->runtime = quux_runtime_create();
    quux_context_initialize(&fixture->context, fixture->runtime);
}

static void close_fixture(fixture *fixture) {
    quux_context_finish(&fixture->context);
    quux_runtime_destroy(fixture->runtime);
}

static void vector_across_level_boundaries(quux_unit_state *state) {
    fixture fixture;
    open_fixture(&fixture);
    quux_context *context = &fixture.context;
    quux_value vector = quux_nothing;
    quux_value updated = quux_nothing;
    QUUX_ROOT_FRAME(context, &vector, &updated);
    QUUX_CHECK(state, quux_vector_empty(context, &vector));
    const int64_t total = 40000; /* crosses 32, 1024 and 32768 */
    bool built = true;
    for (int64_t i = 0; i < total && built; i++) {
        built = quux_vector_add(context, vector, quux_small_integer(i), &vector);
    }
    QUUX_CHECK(state, built);
    QUUX_CHECK(state, quux_vector_count(vector) == (uint64_t)total);
    bool all_match = true;
    for (int64_t i = 0; i < total; i++) {
        all_match = all_match && quux_integer_value(quux_vector_element(vector, (uint64_t)i)) == i;
    }
    QUUX_CHECK(state, all_match);
    QUUX_CHECK(state, quux_vector_put(context, vector, 1500, quux_small_integer(-1), &updated));
    QUUX_CHECK(state, quux_integer_value(quux_vector_element(updated, 1500)) == -1);
    QUUX_CHECK(state, quux_integer_value(quux_vector_element(vector, 1500)) == 1500);
    QUUX_ROOT_FRAME_END(context);
    close_fixture(&fixture);
}

/* Builds a map with keys 0..count-1 inserted in the order given by `step`. */
static bool build_map(quux_context *context, int64_t count, int64_t step, quux_value *result) {
    quux_value map = quux_nothing;
    QUUX_ROOT_FRAME(context, &map);
    bool done = quux_map_empty(context, &map);
    for (int64_t i = 0; i < count && done; i++) {
        int64_t key = (i * step) % count;
        done =
            quux_map_put(context, map, quux_small_integer(key), quux_small_integer(key * 10), &map);
    }
    *result = map;
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static void champ_is_insertion_order_independent(quux_unit_state *state) {
    fixture fixture;
    open_fixture(&fixture);
    quux_context *context = &fixture.context;
    quux_value forward = quux_nothing;
    quux_value shuffled = quux_nothing;
    quux_value found = quux_nothing;
    QUUX_ROOT_FRAME(context, &forward, &shuffled, &found);
    QUUX_CHECK(state, build_map(context, 5000, 1, &forward));
    QUUX_CHECK(state, build_map(context, 5000, 7919, &shuffled)); /* 7919 is prime */
    QUUX_CHECK(state, quux_map_count(forward) == 5000);
    QUUX_CHECK(state, quux_map_count(shuffled) == 5000);
    QUUX_CHECK(state, quux_equal(context, forward, shuffled));
    QUUX_CHECK(state, quux_value_hash(context, forward) == quux_value_hash(context, shuffled));
    QUUX_CHECK(state, quux_map_lookup(context, shuffled, quux_small_integer(4321), &found));
    QUUX_CHECK(state, quux_integer_value(found) == 43210);
    QUUX_CHECK(state, !quux_map_lookup(context, shuffled, quux_small_integer(5000), &found));
    QUUX_ROOT_FRAME_END(context);
    close_fixture(&fixture);
}

static void champ_canonical_after_removals(quux_unit_state *state) {
    fixture fixture;
    open_fixture(&fixture);
    quux_context *context = &fixture.context;
    quux_value small = quux_nothing;
    quux_value large = quux_nothing;
    QUUX_ROOT_FRAME(context, &small, &large);
    QUUX_CHECK(state, build_map(context, 1000, 1, &small));
    QUUX_CHECK(state, build_map(context, 3000, 1, &large));
    bool removed = true;
    for (int64_t key = 2999; key >= 1000 && removed; key--) {
        removed = quux_map_remove(context, large, quux_small_integer(key), &large);
    }
    QUUX_CHECK(state, removed);
    QUUX_CHECK(state, quux_map_count(large) == 1000);
    QUUX_CHECK(state, quux_equal(context, small, large));
    QUUX_CHECK(state, quux_value_hash(context, small) == quux_value_hash(context, large));
    QUUX_ROOT_FRAME_END(context);
    close_fixture(&fixture);
}

static void lists_and_vectors_differ(quux_unit_state *state) {
    fixture fixture;
    open_fixture(&fixture);
    quux_context *context = &fixture.context;
    quux_value list = quux_nothing;
    quux_value vector = quux_nothing;
    QUUX_ROOT_FRAME(context, &list, &vector);
    QUUX_CHECK(state, quux_list_prepend(context, quux_small_integer(2), list, &list));
    QUUX_CHECK(state, quux_list_prepend(context, quux_small_integer(1), list, &list));
    QUUX_CHECK(state, quux_vector_empty(context, &vector));
    QUUX_CHECK(state, quux_vector_add(context, vector, quux_small_integer(1), &vector));
    QUUX_CHECK(state, quux_vector_add(context, vector, quux_small_integer(2), &vector));
    QUUX_CHECK(state, quux_list_count(list) == 2);
    QUUX_CHECK(state, !quux_equal(context, list, vector));
    QUUX_ROOT_FRAME_END(context);
    close_fixture(&fixture);
}

QUUX_UNIT_CASES(collections, {"vector across level boundaries", vector_across_level_boundaries},
                {"CHAMP is insertion-order independent", champ_is_insertion_order_independent},
                {"CHAMP is canonical after removals", champ_canonical_after_removals},
                {"lists and vectors are different kinds", lists_and_vectors_differ});
