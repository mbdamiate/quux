/* Reuse: keep. Reads quux source text into quux data (sections 01-02): lists, vectors, maps, sets,
 * strings, numbers, keywords, symbols and the literal names. There are no reader shorthands
 * (naming rules R5, R6): ' ` ~ @ are rejected. */
#ifndef QUUX_READER_H
#define QUUX_READER_H

#include "quux_values.h"

#include <stddef.h>

typedef struct quux_reader {
    const char *text;
    size_t length;
    size_t position; /* byte offset of the next character */
    size_t line;     /* 1-based line of `position` */
} quux_reader;

typedef enum quux_read_status {
    QUUX_READ_FORM,       /* a form was read into *result */
    QUUX_READ_END,        /* only whitespace and comments were left */
    QUUX_READ_INCOMPLETE, /* the text ends inside a form (the REPL asks for more lines) */
    QUUX_READ_ERROR,      /* a syntax error; the pending error is set */
} quux_read_status;

void quux_reader_initialize(quux_reader *reader, const char *text, size_t length);
/* Skips whitespace and comments. */
void quux_reader_skip_blank(quux_reader *reader);
quux_read_status quux_reader_read(quux_context *context, quux_reader *reader, quux_value *result);
/* Reads exactly one form from the whole text (used by parse functions and tests). */
bool quux_read_one(quux_context *context, const char *text, size_t length, quux_value *result);

#endif
