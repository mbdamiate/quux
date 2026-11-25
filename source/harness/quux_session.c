#include "quux_session.h"

#include "quux_namespaces.h"
#include "quux_printer.h"
#include "quux_reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { QUUX_SESSION_STACK_BYTES = 512u * 1024u * 1024u };

typedef struct session {
    quux_session_body body;
    void *data;
    int status;
} session;

static void *run_session(void *argument) {
    session *current = argument;
    quux_runtime *runtime = quux_runtime_create();
    if (runtime == NULL) {
        current->status = 2;
        return NULL;
    }
    quux_context context;
    quux_context_initialize(&context, runtime);
    if (!quux_evaluator_initialize(&context)) {
        fprintf(stderr, "quux: start-up failed: ");
        quux_session_print_error(&context, stderr, context.pending_error);
        current->status = 2;
    } else {
        current->status = current->body(&context, current->data);
    }
    quux_context_finish(&context);
    quux_runtime_destroy(runtime);
    return NULL;
}

int quux_session_run(quux_session_body body, void *data) {
    session current = {body, data, 2};
    quux_thread *thread = NULL;
    if (!quux_thread_start(&thread, QUUX_SESSION_STACK_BYTES, run_session, &current)) {
        fprintf(stderr, "quux: cannot start the interpreter thread\n");
        return 2;
    }
    quux_thread_join(thread);
    return current.status;
}

void quux_session_print_error(quux_context *context, FILE *stream, quux_value error) {
    if (!quux_is_kind(error, QUUX_KIND_ERROR)) {
        fprintf(stream, "!! error\n");
        return;
    }
    fputs("!! ", stream);
    quux_print_to_stream(context, stream, quux_error_field(error, QUUX_ERROR_KIND),
                         QUUX_PRINT_READABLE);
    fputs(": ", stream);
    quux_print_to_stream(context, stream, quux_error_field(error, QUUX_ERROR_MESSAGE),
                         QUUX_PRINT_DISPLAY);
    fputs("\n", stream);
}

char *quux_session_read_file(const char *path, size_t *length) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    size_t capacity = 4096;
    size_t used = 0;
    char *buffer = malloc(capacity);
    while (buffer != NULL) {
        size_t read = fread(buffer + used, 1, capacity - used, file);
        used += read;
        if (used < capacity) {
            break;
        }
        capacity *= 2;
        char *grown = realloc(buffer, capacity);
        if (grown == NULL) {
            free(buffer);
            buffer = NULL;
        } else {
            buffer = grown;
        }
    }
    fclose(file);
    if (buffer != NULL) {
        *length = used;
    }
    return buffer;
}

typedef struct file_run {
    const char *path;
    const char *input;
    quux_verify_mode verify_mode;
} file_run;

static int run_file(quux_context *context, void *data) {
    file_run *run = data;
    size_t length = 0;
    char *text = quux_session_read_file(run->path, &length);
    if (text == NULL) {
        fprintf(stderr, "quux: cannot read %s\n", run->path);
        return 2;
    }
    quux_value value = quux_nothing;
    quux_value input = quux_nothing;
    quux_value main_function = quux_nothing;
    quux_value definition = quux_nothing;
    QUUX_ROOT_FRAME(context, &value, &input, &main_function, &definition);
    int status = 0;
    quux_memory_set_verify_mode(context->runtime->heap, run->verify_mode);
    bool done = quux_evaluate_text(context, text, length, &value);
    if (done && run->input != NULL) {
        done = quux_read_one(context, run->input, strlen(run->input), &input);
        if (!done) {
            /* a malformed INPUT is a read error: status 2 (command-line contract) */
            status = 2;
        }
    }
    if (done) {
        done = quux_namespace_resolve(context, context->current_namespace,
                                      quux_symbol(context, "main"), &definition);
        if (done && quux_is_nothing(definition)) {
            fprintf(stderr, "quux: %s does not define main\n", run->path);
            status = 2;
        } else if (done) {
            main_function = quux_definition_field(definition, QUUX_DEFINITION_VALUE);
            done = quux_call1(context, main_function, input, &value);
            if (done) {
                quux_print_to_stream(context, stdout, value, QUUX_PRINT_READABLE);
                fputs("\n", stdout);
            }
        }
    }
    if (!done) {
        quux_session_print_error(context, stdout, context->pending_error);
        if (status == 0) {
            status = 1;
        }
    }
    quux_memory_set_verify_mode(context->runtime->heap, QUUX_VERIFY_OFF);
    QUUX_ROOT_FRAME_END(context);
    free(text);
    return status;
}

int quux_session_run_file(const char *path, const char *input_text, quux_verify_mode verify_mode) {
    file_run run = {path, input_text, verify_mode};
    return quux_session_run(run_file, &run);
}
