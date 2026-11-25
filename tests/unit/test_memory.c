#include "quux_unit_test.h"
#include "quux_values.h"

#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static void frames_are_balanced(quux_unit_state *state) {
    quux_runtime *runtime = quux_runtime_create();
    quux_context context;
    quux_context_initialize(&context, runtime);
    quux_root_frame *before = context.roots.top;
    {
        quux_value first = quux_nothing;
        quux_value second = quux_nothing;
        QUUX_ROOT_FRAME(&context, &first, &second);
        QUUX_CHECK(state, context.roots.top != before);
        QUUX_CHECK(state, context.roots.top->count == 2);
        QUUX_ROOT_FRAME_END(&context);
    }
    QUUX_CHECK(state, context.roots.top == before);
    quux_context_finish(&context);
    quux_runtime_destroy(runtime);
}

static void leave_frame_open(quux_context *context) {
    quux_value value = quux_nothing;
    QUUX_ROOT_FRAME(context, &value);
    /* deliberately missing QUUX_ROOT_FRAME_END */
}

static void unclosed_frame_is_detected(quux_unit_state *state) {
#ifdef NDEBUG
    (void)state; /* the check is compiled out in release builds */
#else
    fflush(NULL);
    pid_t child = fork();
    if (child == 0) {
        quux_runtime *runtime = quux_runtime_create();
        quux_context context;
        quux_context_initialize(&context, runtime);
        quux_value outer = quux_nothing;
        QUUX_ROOT_FRAME(&context, &outer);
        leave_frame_open(&context);
        QUUX_ROOT_FRAME_END(&context); /* must abort: the inner frame is still open */
        _exit(0);
    }
    int status = 0;
    waitpid(child, &status, 0);
    QUUX_CHECK(state, WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
#endif
}

static void verifier_releases_unregistered_values(quux_unit_state *state) {
    quux_runtime *runtime = quux_runtime_create();
    quux_context context;
    quux_context_initialize(&context, runtime);
    quux_value registered = quux_nothing;
    quux_value unregistered = quux_nothing;
    QUUX_ROOT_FRAME(&context, &registered);
    QUUX_CHECK(state, quux_make_string_from_c(&context, "kept", &registered));
    QUUX_CHECK(state, quux_make_string_from_c(&context, "lost", &unregistered));
    size_t before = quux_memory_live_object_count(runtime->heap);
    size_t released = quux_memory_verify_roots(runtime->heap);
    QUUX_CHECK(state, released >= 1);
    QUUX_CHECK(state, quux_memory_live_object_count(runtime->heap) == before - released);
    QUUX_CHECK(state, quux_string_length(registered) == 4); /* still valid */
    QUUX_ROOT_FRAME_END(&context);
    quux_context_finish(&context);
    quux_runtime_destroy(runtime);
}

static void allocation_failure_raises_resource_exhausted(quux_unit_state *state) {
    quux_runtime *runtime = quux_runtime_create();
    quux_context context;
    quux_context_initialize(&context, runtime);
    quux_value text = quux_nothing;
    quux_memory_set_allocation_limit(runtime->heap, 1);
    QUUX_CHECK(state, quux_make_string_from_c(&context, "first", &text));
    QUUX_CHECK(state, !quux_make_string_from_c(&context, "second", &text));
    QUUX_CHECK(state, quux_pending_error_is(&context, "resource-exhausted"));
    quux_memory_set_allocation_limit(runtime->heap, 0);
    quux_context_finish(&context);
    quux_runtime_destroy(runtime);
}

static void shutdown_frees_everything(quux_unit_state *state) {
    quux_runtime *runtime = quux_runtime_create();
    quux_context context;
    quux_context_initialize(&context, runtime);
    quux_value text = quux_nothing;
    for (int i = 0; i < 1000; i++) {
        QUUX_CHECK(state, quux_make_string_from_c(&context, "garbage", &text));
    }
    quux_context_finish(&context);
    quux_runtime_destroy(runtime); /* LeakSanitizer and valgrind check that nothing is left */
    QUUX_CHECK(state, true);
}

QUUX_UNIT_CASES(memory, {"root frames are balanced", frames_are_balanced},
                {"an unclosed frame is detected", unclosed_frame_is_detected},
                {"the verifier releases unregistered values",
                 verifier_releases_unregistered_values},
                {"allocation failure raises :resource-exhausted",
                 allocation_failure_raises_resource_exhausted},
                {"shutdown frees every allocation", shutdown_frees_everything});
