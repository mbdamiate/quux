#include "quux_printer.h"

#include "quux_collections.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static quux_function_namer function_namer;

void quux_printer_set_function_namer(quux_function_namer namer) {
    function_namer = namer;
}

void quux_text_initialize(quux_text *text) {
    text->bytes = NULL;
    text->length = 0;
    text->capacity = 0;
    text->failed = false;
}

void quux_text_release(quux_text *text) {
    free(text->bytes);
    quux_text_initialize(text);
}

void quux_text_append(quux_text *text, const char *bytes, size_t length) {
    if (text->failed) {
        return;
    }
    if (text->length + length + 1 > text->capacity) {
        size_t capacity = text->capacity == 0 ? 64 : text->capacity;
        while (text->length + length + 1 > capacity) {
            capacity *= 2;
        }
        char *grown = realloc(text->bytes, capacity);
        if (grown == NULL) {
            text->failed = true;
            return;
        }
        text->bytes = grown;
        text->capacity = capacity;
    }
    memcpy(text->bytes + text->length, bytes, length);
    text->length += length;
    text->bytes[text->length] = '\0';
}

void quux_text_append_c(quux_text *text, const char *bytes) {
    quux_text_append(text, bytes, strlen(bytes));
}

void quux_text_append_format(quux_text *text, const char *format, ...) {
    char buffer[256];
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(buffer, sizeof buffer, format, arguments);
    va_end(arguments);
    if (length < 0) {
        return;
    }
    if ((size_t)length < sizeof buffer) {
        quux_text_append(text, buffer, (size_t)length);
        return;
    }
    char *large = malloc((size_t)length + 1);
    if (large == NULL) {
        text->failed = true;
        return;
    }
    va_start(arguments, format);
    vsnprintf(large, (size_t)length + 1, format, arguments);
    va_end(arguments);
    quux_text_append(text, large, (size_t)length);
    free(large);
}

/* The shortest decimal that reads back as the same double, always with a '.' or an exponent so
 * that it reads back as a float. */
static void print_float(quux_text *text, double number) {
    if (isnan(number)) {
        quux_text_append_c(text, "not-a-number");
        return;
    }
    if (isinf(number)) {
        quux_text_append_c(text, number > 0 ? "infinity" : "negative-infinity");
        return;
    }
    char buffer[64];
    for (int precision = 1; precision <= 17; precision++) {
        snprintf(buffer, sizeof buffer, "%.*g", precision, number);
        if (strtod(buffer, NULL) == number) {
            break;
        }
    }
    quux_text_append_c(text, buffer);
    if (strpbrk(buffer, ".eEn") == NULL) {
        quux_text_append_c(text, ".0");
    }
}

static void print_string_readable(quux_text *text, quux_value string) {
    const char *bytes = quux_string_bytes(string);
    size_t length = quux_string_length(string);
    quux_text_append_c(text, "\"");
    for (size_t i = 0; i < length; i++) {
        switch (bytes[i]) {
        case '"':
            quux_text_append_c(text, "\\\"");
            break;
        case '\\':
            quux_text_append_c(text, "\\\\");
            break;
        case '\n':
            quux_text_append_c(text, "\\n");
            break;
        case '\t':
            quux_text_append_c(text, "\\t");
            break;
        case '\r':
            quux_text_append_c(text, "\\r");
            break;
        default:
            quux_text_append(text, bytes + i, 1);
            break;
        }
    }
    quux_text_append_c(text, "\"");
}

typedef struct entry_printer {
    quux_context *context;
    quux_text *text;
    bool first;
    bool is_map;
} entry_printer;

static void print_value(quux_context *context, quux_text *text, quux_value value);

static bool print_entry(void *data, quux_value key, quux_value value) {
    entry_printer *printer = data;
    if (!printer->first) {
        quux_text_append_c(printer->text, " ");
    }
    printer->first = false;
    print_value(printer->context, printer->text, key);
    if (printer->is_map) {
        quux_text_append_c(printer->text, " ");
        print_value(printer->context, printer->text, value);
    }
    return true;
}

static void print_value(quux_context *context, quux_text *text, quux_value value) {
    quux_kind kind = quux_kind_of(value);
    switch (kind) {
    case QUUX_KIND_NOTHING:
        quux_text_append_c(text, "nothing");
        return;
    case QUUX_KIND_BOOLEAN:
        quux_text_append_c(text, value == quux_true ? "true" : "false");
        return;
    case QUUX_KIND_INTEGER:
        quux_text_append_format(text, "%lld", (long long)quux_integer_value(value));
        return;
    case QUUX_KIND_FLOAT:
        print_float(text, quux_float_value(value));
        return;
    case QUUX_KIND_STRING:
        print_string_readable(text, value);
        return;
    case QUUX_KIND_SYMBOL:
        quux_text_append(text, quux_name_bytes(value), quux_name_length(value));
        return;
    case QUUX_KIND_KEYWORD:
        quux_text_append_c(text, ":");
        quux_text_append(text, quux_name_bytes(value), quux_name_length(value));
        return;
    case QUUX_KIND_PAIR:
        quux_text_append_c(text, "(");
        for (quux_value cell = value; !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
            print_value(context, text, quux_pair_first(cell));
            if (!quux_is_nothing(quux_pair_rest(cell))) {
                quux_text_append_c(text, " ");
            }
        }
        quux_text_append_c(text, ")");
        return;
    case QUUX_KIND_VECTOR: {
        quux_text_append_c(text, "[");
        uint64_t count = quux_vector_count(value);
        for (uint64_t i = 0; i < count; i++) {
            if (i > 0) {
                quux_text_append_c(text, " ");
            }
            print_value(context, text, quux_vector_element(value, i));
        }
        quux_text_append_c(text, "]");
        return;
    }
    case QUUX_KIND_MAP:
    case QUUX_KIND_SET: {
        entry_printer printer = {context, text, true, kind == QUUX_KIND_MAP};
        quux_text_append_c(text, kind == QUUX_KIND_MAP ? "{" : "#{");
        quux_map_for_each(value, print_entry, &printer);
        quux_text_append_c(text, "}");
        return;
    }
    case QUUX_KIND_ERROR:
        quux_text_append_c(text, "(error ");
        print_value(context, text, quux_error_field(value, QUUX_ERROR_KIND));
        quux_text_append_c(text, " ");
        print_value(context, text, quux_error_field(value, QUUX_ERROR_MESSAGE));
        quux_text_append_c(text, " ");
        print_value(context, text, quux_error_field(value, QUUX_ERROR_DATA));
        quux_text_append_c(text, ")");
        return;
    case QUUX_KIND_CLOSURE:
    case QUUX_KIND_PRIMITIVE:
    case QUUX_KIND_MACRO:
        quux_text_append_c(text, kind == QUUX_KIND_MACRO ? "#macro[" : "#function[");
        if (function_namer == NULL || !function_namer(value, text)) {
            quux_text_append_c(text, "anonymous");
        }
        quux_text_append_c(text, "]");
        return;
    default:
        quux_text_append_format(text, "#%s[%llx]", quux_kind_name(kind),
                                (unsigned long long)(value & 0xFFFFFFu));
        return;
    }
}

void quux_print(quux_context *context, quux_text *text, quux_value value, quux_print_style style) {
    if (style == QUUX_PRINT_DISPLAY && quux_kind_of(value) == QUUX_KIND_STRING) {
        quux_text_append(text, quux_string_bytes(value), quux_string_length(value));
        return;
    }
    print_value(context, text, value);
}

bool quux_print_to_string(quux_context *context, quux_value value, quux_print_style style,
                          quux_value *result) {
    quux_text text;
    quux_text_initialize(&text);
    quux_print(context, &text, value, style);
    bool done = !text.failed && quux_make_string(context, text.bytes == NULL ? "" : text.bytes,
                                                 text.length, result);
    if (text.failed) {
        (void)quux_raise(context, "resource-exhausted", "out of memory while printing");
    }
    quux_text_release(&text);
    return done;
}

void quux_print_to_stream(quux_context *context, FILE *stream, quux_value value,
                          quux_print_style style) {
    quux_text text;
    quux_text_initialize(&text);
    quux_print(context, &text, value, style);
    if (text.bytes != NULL) {
        fwrite(text.bytes, 1, text.length, stream);
    }
    quux_text_release(&text);
}
