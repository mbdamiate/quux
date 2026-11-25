#include "quux_reader.h"

#include "quux_collections.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void quux_reader_initialize(quux_reader *reader, const char *text, size_t length) {
    reader->text = text;
    reader->length = length;
    reader->position = 0;
    reader->line = 1;
}

static bool at_end(const quux_reader *reader) {
    return reader->position >= reader->length;
}

static char peek(const quux_reader *reader) {
    return at_end(reader) ? '\0' : reader->text[reader->position];
}

static char advance(quux_reader *reader) {
    char character = reader->text[reader->position++];
    if (character == '\n') {
        reader->line++;
    }
    return character;
}

static bool is_blank(char character) {
    return character == ' ' || character == '\t' || character == '\n' || character == '\r';
}

static bool is_delimiter(char character) {
    return is_blank(character) || character == '(' || character == ')' || character == '[' ||
           character == ']' || character == '{' || character == '}' || character == '"' ||
           character == ';' || character == '\0';
}

void quux_reader_skip_blank(quux_reader *reader) {
    while (!at_end(reader)) {
        char character = peek(reader);
        if (is_blank(character)) {
            advance(reader);
        } else if (character == ';') {
            while (!at_end(reader) && peek(reader) != '\n') {
                advance(reader);
            }
        } else {
            return;
        }
    }
}

/* Text the reader cannot read raises :syntax (D-54). */
static quux_read_status syntax_error(quux_context *context, const quux_reader *reader,
                                     const char *message) {
    (void)quux_raise(context, "syntax", "line %zu: %s", reader->line, message);
    return QUUX_READ_ERROR;
}

static quux_read_status read_form(quux_context *context, quux_reader *reader, quux_value *result);

/* Reads forms up to `closing`, collecting them into a list in reverse. */
static quux_read_status read_sequence(quux_context *context, quux_reader *reader, char closing,
                                      quux_value *reversed) {
    quux_value element = quux_nothing;
    QUUX_ROOT_FRAME(context, &element);
    quux_read_status status = QUUX_READ_FORM;
    for (;;) {
        quux_reader_skip_blank(reader);
        if (at_end(reader)) {
            status = QUUX_READ_INCOMPLETE;
            break;
        }
        char character = peek(reader);
        if (character == closing) {
            advance(reader);
            break;
        }
        if (character == ')' || character == ']' || character == '}') {
            status = syntax_error(context, reader, "unbalanced closing bracket");
            break;
        }
        status = read_form(context, reader, &element);
        if (status != QUUX_READ_FORM) {
            break;
        }
        if (!quux_list_prepend(context, element, *reversed, reversed)) {
            status = QUUX_READ_ERROR;
            break;
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return status;
}

static quux_read_status read_string(quux_context *context, quux_reader *reader,
                                    quux_value *result) {
    advance(reader); /* opening quote */
    size_t capacity = 64;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        return syntax_error(context, reader, "out of memory");
    }
    quux_read_status status = QUUX_READ_FORM;
    for (;;) {
        if (at_end(reader)) {
            status = QUUX_READ_INCOMPLETE;
            break;
        }
        char character = advance(reader);
        if (character == '"') {
            break;
        }
        if (character == '\\') {
            if (at_end(reader)) {
                status = QUUX_READ_INCOMPLETE;
                break;
            }
            char escaped = advance(reader);
            switch (escaped) {
            case 'n':
                character = '\n';
                break;
            case 't':
                character = '\t';
                break;
            case 'r':
                character = '\r';
                break;
            case '\\':
            case '"':
                character = escaped;
                break;
            default:
                status = syntax_error(context, reader, "unknown escape in string");
                break;
            }
            if (status != QUUX_READ_FORM) {
                break;
            }
        }
        if (length + 1 >= capacity) {
            capacity *= 2;
            char *grown = realloc(buffer, capacity);
            if (grown == NULL) {
                status = syntax_error(context, reader, "out of memory");
                break;
            }
            buffer = grown;
        }
        buffer[length++] = character;
    }
    if (status == QUUX_READ_FORM && !quux_make_string(context, buffer, length, result)) {
        status = QUUX_READ_ERROR;
    }
    free(buffer);
    return status;
}

static bool looks_numeric(const char *token, size_t length) {
    size_t start = (token[0] == '-' || token[0] == '+') ? 1 : 0;
    return start < length && token[start] >= '0' && token[start] <= '9';
}

static quux_read_status read_number(quux_context *context, const quux_reader *reader,
                                    const char *token, size_t length, quux_value *result) {
    char buffer[128];
    if (length >= sizeof buffer) {
        return syntax_error(context, reader, "number too long");
    }
    memcpy(buffer, token, length);
    buffer[length] = '\0';
    bool is_float = strpbrk(buffer, ".eE") != NULL;
    char *end = NULL;
    errno = 0;
    if (is_float) {
        double number = strtod(buffer, &end);
        if (*end != '\0') {
            return syntax_error(context, reader, "malformed number");
        }
        return quux_make_float(context, number, result) ? QUUX_READ_FORM : QUUX_READ_ERROR;
    }
    long long number = strtoll(buffer, &end, 10);
    if (*end != '\0') {
        return syntax_error(context, reader, "malformed number");
    }
    if (errno == ERANGE) {
        (void)quux_raise(context, "overflow", "integer literal %s does not fit in 64 bits", buffer);
        return QUUX_READ_ERROR;
    }
    return quux_make_integer(context, (int64_t)number, result) ? QUUX_READ_FORM : QUUX_READ_ERROR;
}

static quux_read_status read_atom(quux_context *context, quux_reader *reader, quux_value *result) {
    size_t start = reader->position;
    while (!at_end(reader) && !is_delimiter(peek(reader))) {
        advance(reader);
    }
    const char *token = reader->text + start;
    size_t length = reader->position - start;
    if (token[0] == '\'' || token[0] == '`' || token[0] == '~' || token[0] == '@') {
        return syntax_error(context, reader,
                            "reader shorthands do not exist; write (quote x), (quasi-quote x), "
                            "(unquote x) or (wait-for x)");
    }
    if (token[0] == ':') {
        if (length < 2) {
            return syntax_error(context, reader, "empty keyword");
        }
        return quux_intern_keyword(context, token + 1, length - 1, result) ? QUUX_READ_FORM
                                                                           : QUUX_READ_ERROR;
    }
    if (looks_numeric(token, length)) {
        return read_number(context, reader, token, length, result);
    }
    static const struct {
        const char *name;
        int index;
    } literals[] = {{"nothing", 0},      {"false", 1},    {"true", 2},
                    {"not-a-number", 3}, {"infinity", 4}, {"negative-infinity", 5}};
    for (size_t i = 0; i < sizeof literals / sizeof literals[0]; i++) {
        if (strlen(literals[i].name) == length && memcmp(literals[i].name, token, length) == 0) {
            switch (literals[i].index) {
            case 0:
                *result = quux_nothing;
                return QUUX_READ_FORM;
            case 1:
                *result = quux_false;
                return QUUX_READ_FORM;
            case 2:
                *result = quux_true;
                return QUUX_READ_FORM;
            case 3:
                return quux_make_float(context, NAN, result) ? QUUX_READ_FORM : QUUX_READ_ERROR;
            case 4:
                return quux_make_float(context, INFINITY, result) ? QUUX_READ_FORM
                                                                  : QUUX_READ_ERROR;
            default:
                return quux_make_float(context, -INFINITY, result) ? QUUX_READ_FORM
                                                                   : QUUX_READ_ERROR;
            }
        }
    }
    if (memchr(token, '#', length) != NULL) {
        return syntax_error(context, reader, "'#' may only start a set literal #{...}");
    }
    return quux_intern_symbol(context, token, length, result) ? QUUX_READ_FORM : QUUX_READ_ERROR;
}

static quux_read_status read_form(quux_context *context, quux_reader *reader, quux_value *result) {
    quux_reader_skip_blank(reader);
    if (at_end(reader)) {
        return QUUX_READ_END;
    }
    char character = peek(reader);
    quux_value reversed = quux_nothing;
    quux_value collection = quux_nothing;
    quux_value key = quux_nothing;
    quux_read_status status = QUUX_READ_FORM;
    QUUX_ROOT_FRAME(context, &reversed, &collection, &key);
    switch (character) {
    case '(':
        advance(reader);
        status = read_sequence(context, reader, ')', &reversed);
        if (status == QUUX_READ_FORM && !quux_list_reverse(context, reversed, result)) {
            status = QUUX_READ_ERROR;
        }
        break;
    case '[':
        advance(reader);
        status = read_sequence(context, reader, ']', &reversed);
        if (status == QUUX_READ_FORM) {
            bool done = quux_list_reverse(context, reversed, &reversed) &&
                        quux_vector_empty(context, &collection);
            for (quux_value cell = reversed; done && !quux_is_nothing(cell);
                 cell = quux_pair_rest(cell)) {
                done = quux_vector_add(context, collection, quux_pair_first(cell), &collection);
            }
            status = done ? QUUX_READ_FORM : QUUX_READ_ERROR;
            *result = collection;
        }
        break;
    case '{':
    case '#': {
        bool is_set = character == '#';
        advance(reader);
        if (is_set) {
            if (peek(reader) != '{') {
                status = syntax_error(context, reader, "'#' may only start a set literal #{...}");
                break;
            }
            advance(reader);
        }
        status = read_sequence(context, reader, '}', &reversed);
        if (status != QUUX_READ_FORM) {
            break;
        }
        bool done =
            quux_list_reverse(context, reversed, &reversed) &&
            (is_set ? quux_set_empty(context, &collection) : quux_map_empty(context, &collection));
        if (done && !is_set && quux_list_count(reversed) % 2 != 0) {
            status = syntax_error(context, reader, "a map literal needs an even number of forms");
            break;
        }
        quux_value cell = reversed;
        while (done && !quux_is_nothing(cell)) {
            if (is_set) {
                done = quux_map_put(context, collection, quux_pair_first(cell),
                                    quux_pair_first(cell), &collection);
                cell = quux_pair_rest(cell);
            } else {
                key = quux_pair_first(cell);
                cell = quux_pair_rest(cell);
                done = quux_map_put(context, collection, key, quux_pair_first(cell), &collection);
                cell = quux_pair_rest(cell);
            }
        }
        status = done ? QUUX_READ_FORM : QUUX_READ_ERROR;
        *result = collection;
        break;
    }
    case ')':
    case ']':
    case '}':
        advance(reader);
        status = syntax_error(context, reader, "unbalanced closing bracket");
        break;
    case '"':
        status = read_string(context, reader, result);
        break;
    default:
        status = read_atom(context, reader, result);
        break;
    }
    QUUX_ROOT_FRAME_END(context);
    return status;
}

quux_read_status quux_reader_read(quux_context *context, quux_reader *reader, quux_value *result) {
    return read_form(context, reader, result);
}

bool quux_read_one(quux_context *context, const char *text, size_t length, quux_value *result) {
    quux_reader reader;
    quux_reader_initialize(&reader, text, length);
    quux_read_status status = quux_reader_read(context, &reader, result);
    if (status == QUUX_READ_FORM) {
        quux_reader_skip_blank(&reader);
        if (at_end(&reader)) {
            return true;
        }
        return syntax_error(context, &reader, "more than one form") == QUUX_READ_FORM;
    }
    if (status == QUUX_READ_END || status == QUUX_READ_INCOMPLETE) {
        (void)syntax_error(context, &reader, "incomplete or empty text");
    }
    return false;
}
