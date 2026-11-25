/* quux: the REPL and the file runner (D-52, contracts/command-line.md).
 *   quux                  REPL
 *   quux FILE.qx [INPUT]  loads FILE.qx and calls (main input) */
#include "quux_namespaces.h"
#include "quux_printer.h"
#include "quux_reader.h"
#include "quux_session.h"

#include <stdlib.h>
#include <string.h>

static int run_repl(quux_context *context, void *data) {
    (void)data;
    bool interactive = quux_standard_input_is_terminal();
    size_t capacity = 4096;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        return 2;
    }
    char line[4096];
    quux_value form = quux_nothing;
    quux_value value = quux_nothing;
    QUUX_ROOT_FRAME(context, &form, &value);
    for (;;) {
        if (interactive) {
            fputs(length == 0 ? "quux> " : "  ... ", stdout);
            fflush(stdout);
        }
        if (fgets(line, sizeof line, stdin) == NULL) {
            break;
        }
        size_t line_length = strlen(line);
        if (length + line_length + 1 > capacity) {
            while (length + line_length + 1 > capacity) {
                capacity *= 2;
            }
            char *grown = realloc(buffer, capacity);
            if (grown == NULL) {
                break;
            }
            buffer = grown;
        }
        memcpy(buffer + length, line, line_length);
        length += line_length;
        quux_reader reader;
        quux_reader_initialize(&reader, buffer, length);
        size_t consumed = 0;
        bool incomplete = false;
        for (;;) {
            quux_read_status status = quux_reader_read(context, &reader, &form);
            if (status == QUUX_READ_END) {
                consumed = reader.position;
                break;
            }
            if (status == QUUX_READ_INCOMPLETE) {
                incomplete = true;
                break;
            }
            consumed = reader.position;
            if (status == QUUX_READ_ERROR) {
                quux_session_print_error(context, stdout, context->pending_error);
                context->pending_error = quux_nothing;
                consumed = length; /* discard the rest of the faulty input */
                break;
            }
            if (quux_evaluate_form(context, form, &value)) {
                quux_print_to_stream(context, stdout, value, QUUX_PRINT_READABLE);
                fputs("\n", stdout);
            } else {
                quux_session_print_error(context, stdout, context->pending_error);
                context->pending_error = quux_nothing;
            }
            context->recursion_depth = 0;
            fflush(stdout);
        }
        if (!incomplete || consumed > 0) {
            memmove(buffer, buffer + consumed, length - consumed);
            length -= consumed;
        }
    }
    QUUX_ROOT_FRAME_END(context);
    free(buffer);
    return 0;
}

typedef struct file_run {
    const char *path;
    const char *input;
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
    bool done = quux_evaluate_text(context, text, length, &value);
    if (done && run->input != NULL) {
        done = quux_read_one(context, run->input, strlen(run->input), &input);
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
        status = 1;
    }
    QUUX_ROOT_FRAME_END(context);
    free(text);
    return status;
}

int main(int argument_count, char **arguments) {
    if (argument_count == 1) {
        return quux_session_run(run_repl, NULL);
    }
    size_t path_length = strlen(arguments[1]);
    if (argument_count > 3 || path_length < 4 ||
        strcmp(arguments[1] + path_length - 3, ".qx") != 0) {
        fprintf(stderr, "usage: quux [FILE.qx [INPUT]]\n");
        return 2;
    }
    file_run run = {arguments[1], argument_count == 3 ? arguments[2] : NULL};
    return quux_session_run(run_file, &run);
}
