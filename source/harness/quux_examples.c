/* The specification example suite (FR-026, T035). In a ```quux block, a form followed by a
 * comment `;=> expected` must evaluate to a value equal? to `expected` (alternatives may be
 * written `a or b`); a form followed by `;!! :kind` must raise an error of that kind. The comment
 * may be on the form's last line or on a following line before the next form. Forms without
 * either comment are evaluated and must not raise. */
#include "quux_harness.h"

#include "quux_collections.h"
#include "quux_printer.h"
#include "quux_reader.h"
#include "quux_session.h"

#include <stdlib.h>
#include <string.h>

typedef struct block {
    size_t start; /* offset of the block text in the file */
    size_t length;
    size_t first_line; /* line number of the first line of the block text */
    bool skipped;
} block;

typedef struct example_run {
    const char *path;
    const quux_example_options *options;
    char *text;
    size_t length;
    block *blocks;
    size_t block_count;
    int passed;
    int failed;
    int skipped;
} example_run;

static bool feature_available(const char *features, const char *wanted, size_t wanted_length) {
    const char *cursor = features;
    while (cursor != NULL && *cursor != '\0') {
        while (*cursor == ' ') {
            cursor++;
        }
        size_t length = strcspn(cursor, " ");
        if (length == wanted_length && memcmp(cursor, wanted, length) == 0) {
            return true;
        }
        cursor += length;
    }
    return false;
}

static void find_blocks(example_run *run) {
    size_t capacity = 16;
    run->blocks = malloc(capacity * sizeof *run->blocks);
    size_t line_number = 1;
    size_t position = 0;
    const char *requirement = NULL;
    size_t requirement_length = 0;
    bool inside = false;
    block current = {0, 0, 0, false};
    while (position < run->length && run->blocks != NULL) {
        size_t end = position;
        while (end < run->length && run->text[end] != '\n') {
            end++;
        }
        const char *line = run->text + position;
        size_t line_length = end - position;
        const char *marker = "<!-- example-requires:";
        if (!inside && line_length > strlen(marker) && memcmp(line, marker, strlen(marker)) == 0) {
            requirement = line + strlen(marker);
            while (*requirement == ' ') {
                requirement++;
            }
            requirement_length = strcspn(requirement, " -");
        } else if (!inside && line_length >= 7 && memcmp(line, "```quux", 7) == 0) {
            inside = true;
            current.start = end + 1;
            current.first_line = line_number + 1;
            current.skipped =
                requirement != NULL && !feature_available(run->options->available_features,
                                                          requirement, requirement_length);
            requirement = NULL;
        } else if (inside && line_length >= 3 && memcmp(line, "```", 3) == 0) {
            inside = false;
            current.length = position - current.start;
            if (run->block_count == capacity) {
                capacity *= 2;
                block *grown = realloc(run->blocks, capacity * sizeof *grown);
                if (grown == NULL) {
                    break;
                }
                run->blocks = grown;
            }
            run->blocks[run->block_count++] = current;
        } else if (!inside && line_length > 0) {
            requirement = requirement != NULL && line_length > 0 ? NULL : requirement;
        }
        position = end + 1;
        line_number++;
    }
}

typedef enum expectation_kind { EXPECT_NOTHING, EXPECT_VALUE, EXPECT_ERROR } expectation_kind;

/* Finds the ;=> or ;!! comment that belongs to the form ending at `form_end`: on the same line,
 * or on a following comment line before the next form. */
static expectation_kind find_expectation(const char *text, size_t length, size_t form_end,
                                         size_t *expected_start, size_t *expected_length) {
    size_t position = form_end;
    while (position < length) {
        while (position < length && (text[position] == ' ' || text[position] == '\t' ||
                                     text[position] == '\r' || text[position] == '\n')) {
            position++;
        }
        if (position >= length || text[position] != ';') {
            return EXPECT_NOTHING; /* the next form starts, or the block ends */
        }
        size_t line_end = position;
        while (line_end < length && text[line_end] != '\n') {
            line_end++;
        }
        if (line_end - position >= 3 &&
            (memcmp(text + position, ";=>", 3) == 0 || memcmp(text + position, ";!!", 3) == 0)) {
            *expected_start = position + 3;
            /* the expectation ends at the next comment */
            size_t stop = *expected_start;
            bool in_string = false;
            while (stop < line_end && (in_string || text[stop] != ';')) {
                if (text[stop] == '"') {
                    in_string = !in_string;
                }
                stop++;
            }
            *expected_length = stop - *expected_start;
            return text[position + 1] == '=' ? EXPECT_VALUE : EXPECT_ERROR;
        }
        position = line_end;
    }
    return EXPECT_NOTHING;
}

static void report(example_run *run, size_t line, const char *problem, quux_context *context,
                   quux_value shown) {
    printf("  FAILED %s:%zu: %s", run->path, line, problem);
    if (shown != 0) {
        fputs(" ", stdout);
        quux_print_to_stream(context, stdout, shown, QUUX_PRINT_READABLE);
    }
    fputs("\n", stdout);
}

static void describe_error(quux_context *context, char *buffer, size_t size) {
    quux_value error = context->pending_error;
    if (!quux_is_kind(error, QUUX_KIND_ERROR)) {
        snprintf(buffer, size, "(unknown error)");
        return;
    }
    quux_value kind = quux_error_field(error, QUUX_ERROR_KIND);
    quux_value message = quux_error_field(error, QUUX_ERROR_MESSAGE);
    snprintf(buffer, size, ":%.*s %.*s", (int)quux_name_length(kind), quux_name_bytes(kind),
             quux_is_kind(message, QUUX_KIND_STRING) ? (int)quux_string_length(message) : 0,
             quux_is_kind(message, QUUX_KIND_STRING) ? quux_string_bytes(message) : "");
}

/* Evaluates the expected text: one form, or alternatives separated by the word `or`. Returns
 * whether value matches one of them. */
static bool expected_value_matches(quux_context *context, const char *text, size_t length,
                                   quux_value actual, bool *matches) {
    quux_reader reader;
    quux_reader_initialize(&reader, text, length);
    quux_value form = quux_nothing;
    quux_value expected = quux_nothing;
    bool done = true;
    *matches = false;
    QUUX_ROOT_FRAME(context, &actual, &form, &expected);
    for (;;) {
        quux_read_status status = quux_reader_read(context, &reader, &form);
        if (status != QUUX_READ_FORM) {
            done = status == QUUX_READ_END;
            break;
        }
        if (quux_is_kind(form, QUUX_KIND_SYMBOL) && quux_name_equals(form, "or")) {
            continue;
        }
        if (!quux_evaluate_form(context, form, &expected)) {
            done = false;
            break;
        }
        if (quux_equal(context, actual, expected)) {
            *matches = true;
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static void run_block(example_run *run, quux_context *context, const block *current) {
    const char *text = run->text + current->start;
    quux_reader reader;
    quux_reader_initialize(&reader, text, current->length);
    quux_value form = quux_nothing;
    quux_value value = quux_nothing;
    quux_value kind = quux_nothing;
    QUUX_ROOT_FRAME(context, &form, &value, &kind);
    for (;;) {
        quux_reader_skip_blank(&reader);
        size_t form_line = current->first_line + reader.line - 1;
        quux_read_status status = quux_reader_read(context, &reader, &form);
        if (status == QUUX_READ_END) {
            break;
        }
        if (status != QUUX_READ_FORM) {
            report(run, form_line, "the example does not read", context, 0);
            run->failed++;
            context->pending_error = quux_nothing;
            break;
        }
        size_t expected_start = 0;
        size_t expected_length = 0;
        expectation_kind expectation = find_expectation(text, current->length, reader.position,
                                                        &expected_start, &expected_length);
        bool evaluated = quux_session_evaluate_form(context, form, &value);
        context->recursion_depth = 0;
        char description[600];
        switch (expectation) {
        case EXPECT_NOTHING:
            if (evaluated) {
                run->passed++;
            } else {
                describe_error(context, description, sizeof description);
                report(run, form_line, "unexpected error", context, 0);
                printf("         %s\n", description);
                run->failed++;
            }
            break;
        case EXPECT_VALUE: {
            if (!evaluated) {
                describe_error(context, description, sizeof description);
                report(run, form_line, "expected a value, got an error", context, 0);
                printf("         %s\n", description);
                run->failed++;
                break;
            }
            bool matches = false;
            if (!expected_value_matches(context, text + expected_start, expected_length, value,
                                        &matches)) {
                describe_error(context, description, sizeof description);
                report(run, form_line, "the expected value does not evaluate", context, 0);
                printf("         %s\n", description);
                run->failed++;
            } else if (matches) {
                run->passed++;
            } else {
                report(run, form_line, "got", context, value);
                printf("         expected:%.*s\n", (int)expected_length, text + expected_start);
                run->failed++;
            }
            break;
        }
        case EXPECT_ERROR: {
            bool read = quux_read_one(context, text + expected_start, expected_length, &kind);
            if (evaluated) {
                report(run, form_line, "expected an error, got", context, value);
                run->failed++;
            } else if (!read || !quux_is_kind(kind, QUUX_KIND_KEYWORD)) {
                report(run, form_line, "the expected error kind does not read", context, 0);
                run->failed++;
            } else if (quux_is_kind(context->pending_error, QUUX_KIND_ERROR) &&
                       quux_error_field(context->pending_error, QUUX_ERROR_KIND) == kind) {
                run->passed++;
            } else {
                describe_error(context, description, sizeof description);
                report(run, form_line, "wrong error", context, 0);
                printf("         got %s, expected%.*s\n", description, (int)expected_length,
                       text + expected_start);
                run->failed++;
            }
            break;
        }
        }
        context->pending_error = quux_nothing;
    }
    QUUX_ROOT_FRAME_END(context);
}

static int run_examples(quux_context *context, void *data) {
    example_run *run = data;
    FILE *discard = fopen("/dev/null", "w");
    if (discard != NULL) {
        context->runtime->output = discard; /* print-line output is not checked here */
    }
    if (run->options->verify_every_allocation) {
        quux_memory_set_verify_mode(context->runtime->heap, QUUX_VERIFY_EVERY_ALLOCATION);
    } else if (run->options->verify_roots) {
        quux_memory_set_verify_mode(context->runtime->heap, QUUX_VERIFY_AMORTIZED);
    }
    for (size_t i = 0; i < run->block_count; i++) {
        if (run->blocks[i].skipped) {
            run->skipped++;
            continue;
        }
        run_block(run, context, &run->blocks[i]);
    }
    quux_memory_set_verify_mode(context->runtime->heap, QUUX_VERIFY_OFF);
    context->runtime->output = stdout;
    if (discard != NULL) {
        fclose(discard);
    }
    return 0;
}

int quux_run_examples_file(const char *path, const quux_example_options *options) {
    example_run run = {path, options, NULL, 0, NULL, 0, 0, 0, 0};
    run.text = quux_session_read_file(path, &run.length);
    if (run.text == NULL) {
        printf("  FAILED %s: cannot read the file\n", path);
        return 1;
    }
    find_blocks(&run);
    int status = quux_session_run(run_examples, &run);
    printf("%s: %d passed, %d failed, %zu block%s skipped\n", path, run.passed, run.failed,
           (size_t)run.skipped, run.skipped == 1 ? "" : "s");
    free(run.blocks);
    free(run.text);
    if (status != 0) {
        return status;
    }
    return run.failed;
}
