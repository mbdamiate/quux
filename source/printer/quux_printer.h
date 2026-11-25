/* Reuse: keep. Prints values in canonical form: maps and sets in canonical order, so equal values
 * print identically (section 02). */
#ifndef QUUX_PRINTER_H
#define QUUX_PRINTER_H

#include "quux_values.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct quux_text {
    char *bytes;
    size_t length;
    size_t capacity;
    bool failed;
} quux_text;

void quux_text_initialize(quux_text *text);
void quux_text_release(quux_text *text);
void quux_text_append(quux_text *text, const char *bytes, size_t length);
void quux_text_append_c(quux_text *text, const char *bytes);
void quux_text_append_format(quux_text *text, const char *format, ...) QUUX_PRINTF_FORMAT(2, 3);

typedef enum quux_print_style {
    QUUX_PRINT_READABLE, /* strings quoted and escaped (REPL, examples, messages) */
    QUUX_PRINT_DISPLAY,  /* a string at the top level prints its bytes (print-line, to-string) */
} quux_print_style;

/* Appends the printed form of value. Never allocates quux objects. */
void quux_print(quux_context *context, quux_text *text, quux_value value, quux_print_style style);
/* Prints into a new quux string. */
bool quux_print_to_string(quux_context *context, quux_value value, quux_print_style style,
                          quux_value *result);
/* Prints to a C stream. */
void quux_print_to_stream(quux_context *context, FILE *stream, quux_value value,
                          quux_print_style style);

/* A function name for printing #function[name], set by the evaluator. */
typedef bool (*quux_function_namer)(quux_value function, quux_text *text);
void quux_printer_set_function_namer(quux_function_namer namer);

#endif
