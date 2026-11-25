#include "quux_session.h"

#include "quux_printer.h"

#include <stdio.h>
#include <stdlib.h>

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
