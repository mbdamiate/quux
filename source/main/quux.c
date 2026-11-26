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
            if (quux_session_evaluate_form(context, form, &value)) {
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
    return quux_session_run_file(arguments[1], argument_count == 3 ? arguments[2] : NULL,
                                 QUUX_VERIFY_OFF);
}
