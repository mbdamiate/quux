/* quux-name-check (D-47, T037): every public quux name used by the programs and the
 * specification must be approved in docs/naming.md (constitution VIII), and no C identifier may
 * contain an abbreviation that docs/naming.md section H does not allow.
 *   quux-name-check            quux names in every .qx file under programs/ and the quux
 *                              blocks of docs/spec
 *   quux-name-check --c DIR…   C identifiers in .c and .h files
 * Run from the repository root. */
#include "quux_collections.h"
#include "quux_harness.h"
#include "quux_reader.h"
#include "quux_session.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ---- A simple set of C strings ---- */

typedef struct name_set {
    char **names;
    size_t count;
    size_t capacity;
} name_set;

static bool set_contains(const name_set *set, const char *name, size_t length) {
    for (size_t i = 0; i < set->count; i++) {
        if (strlen(set->names[i]) == length && memcmp(set->names[i], name, length) == 0) {
            return true;
        }
    }
    return false;
}

static void set_add(name_set *set, const char *name, size_t length) {
    if (length == 0 || set_contains(set, name, length)) {
        return;
    }
    if (set->count == set->capacity) {
        set->capacity = set->capacity == 0 ? 256 : set->capacity * 2;
        set->names = realloc(set->names, set->capacity * sizeof *set->names);
    }
    set->names[set->count++] = strndup(name, length);
}

static void set_release(name_set *set) {
    for (size_t i = 0; i < set->count; i++) {
        free(set->names[i]);
    }
    free(set->names);
}

/* ---- docs/naming.md ---- */

/* Adds the words inside backtick spans of a table cell. */
static void add_backticked_words(name_set *set, const char *cell, size_t length) {
    size_t i = 0;
    while (i < length) {
        if (cell[i] != '`') {
            i++;
            continue;
        }
        size_t ticks = 0;
        while (i < length && cell[i] == '`') {
            ticks++;
            i++;
        }
        size_t start = i;
        while (i < length && !(cell[i] == '`')) {
            i++;
        }
        size_t end = i;
        while (i < length && cell[i] == '`') {
            i++;
        }
        if (ticks > 1) {
            continue; /* a span showing backticks themselves */
        }
        size_t word_start = start;
        for (size_t j = start; j <= end; j++) {
            bool separator = j == end || isspace((unsigned char)cell[j]) || cell[j] == '(' ||
                             cell[j] == ')' || cell[j] == '[' || cell[j] == ']' || cell[j] == '{' ||
                             cell[j] == '}' || cell[j] == ',';
            if (separator) {
                if (j > word_start) {
                    set_add(set, cell + word_start, j - word_start);
                }
                word_start = j + 1;
            }
        }
    }
}

static bool cell_has_backtick(const char *cell, size_t length) {
    return memchr(cell, '`', length) != NULL;
}

/* Whether `needle` occurs in the first `length` bytes of `text`. */
static bool contains_text(const char *text, size_t length, const char *needle) {
    size_t needle_length = strlen(needle);
    for (size_t i = 0; i + needle_length <= length; i++) {
        if (memcmp(text + i, needle, needle_length) == 0) {
            return true;
        }
    }
    return false;
}

static bool load_glossary(const char *path, name_set *approved, name_set *banned_words) {
    size_t length = 0;
    char *text = quux_session_read_file(path, &length);
    if (text == NULL) {
        fprintf(stderr, "quux-name-check: cannot read %s\n", path);
        return false;
    }
    bool first_column_is_tradition = false;
    char *line = text;
    while (line < text + length) {
        char *end = memchr(line, '\n', (size_t)(text + length - line));
        if (end == NULL) {
            end = text + length;
        }
        size_t line_length = (size_t)(end - line);
        if (line_length > 2 && line[0] == '|') {
            /* split the row into cells */
            const char *cells[8];
            size_t cell_lengths[8];
            size_t cell_count = 0;
            const char *cell = line + 1;
            for (const char *cursor = line + 1; cursor < end && cell_count < 8; cursor++) {
                if (*cursor == '|' && cursor[-1] != '\\') {
                    cells[cell_count] = cell;
                    cell_lengths[cell_count] = (size_t)(cursor - cell);
                    cell_count++;
                    cell = cursor + 1;
                }
            }
            bool header = cell_count > 0 && contains_text(line, line_length, "Status") &&
                          memchr(line, '`', line_length) == NULL;
            if (header) {
                first_column_is_tradition = strncmp(line, "| Lisp tradition", 16) == 0 ||
                                            strncmp(line, "| Earlier draft", 15) == 0;
            } else if (cell_count >= 2) {
                const char *status = cells[cell_count - 1];
                size_t status_length = cell_lengths[cell_count - 1];
                bool is_approved = contains_text(status, status_length, "approved");
                bool not_allowed = contains_text(line, line_length, "**not allowed**");
                if (is_approved && not_allowed) {
                    add_backticked_words(banned_words, cells[0], cell_lengths[0]);
                } else if (is_approved) {
                    if (first_column_is_tradition) {
                        bool renamed_some = cell_has_backtick(cells[1], cell_lengths[1]);
                        bool rest_unchanged = contains_text(cells[1], cell_lengths[1], "unchanged");
                        if (renamed_some && rest_unchanged) {
                            /* "`old` -> `new`; the rest unchanged": the names of the first
                             * column that were not renamed, plus the new names */
                            name_set original = {NULL, 0, 0};
                            name_set renamed = {NULL, 0, 0};
                            add_backticked_words(&original, cells[0], cell_lengths[0]);
                            add_backticked_words(&renamed, cells[1], cell_lengths[1]);
                            for (size_t i = 0; i < original.count; i++) {
                                if (!set_contains(&renamed, original.names[i],
                                                  strlen(original.names[i]))) {
                                    set_add(approved, original.names[i], strlen(original.names[i]));
                                }
                            }
                            for (size_t i = 0; i < renamed.count; i++) {
                                if (!set_contains(&original, renamed.names[i],
                                                  strlen(renamed.names[i]))) {
                                    set_add(approved, renamed.names[i], strlen(renamed.names[i]));
                                }
                            }
                            set_release(&original);
                            set_release(&renamed);
                        } else if (renamed_some) {
                            add_backticked_words(approved, cells[1], cell_lengths[1]);
                        } else {
                            add_backticked_words(approved, cells[0], cell_lengths[0]);
                        }
                    } else {
                        for (size_t i = 0; i + 1 < cell_count; i++) {
                            add_backticked_words(approved, cells[i], cell_lengths[i]);
                        }
                    }
                }
            }
        } else if (line_length == 0) {
            first_column_is_tradition = false;
        }
        line = end + 1;
    }
    free(text);
    return true;
}

/* ---- quux files ---- */

typedef struct check_state {
    quux_context *context;
    name_set *approved;
    name_set bound;
    name_set macros; /* macros defined in the files being checked */
    int offending;
    const char *file;
} check_state;

static bool head_is(quux_value form, const char *name) {
    if (!quux_is_kind(form, QUUX_KIND_PAIR)) {
        return false;
    }
    quux_value head = quux_pair_first(form);
    return quux_is_kind(head, QUUX_KIND_SYMBOL) && quux_name_equals(head, name);
}

static void bind_all_symbols(check_state *state, quux_value value);

typedef struct bind_visitor {
    check_state *state;
} bind_visitor;

static bool bind_entry(void *data, quux_value key, quux_value value) {
    bind_visitor *visitor = data;
    bind_all_symbols(visitor->state, key);
    bind_all_symbols(visitor->state, value);
    return true;
}

static void bind_all_symbols(check_state *state, quux_value value) {
    switch (quux_kind_of(value)) {
    case QUUX_KIND_SYMBOL:
        set_add(&state->bound, quux_name_bytes(value), quux_name_length(value));
        return;
    case QUUX_KIND_PAIR:
        for (quux_value cell = value; !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
            bind_all_symbols(state, quux_pair_first(cell));
        }
        return;
    case QUUX_KIND_VECTOR:
        for (uint64_t i = 0; i < quux_vector_count(value); i++) {
            bind_all_symbols(state, quux_vector_element(value, i));
        }
        return;
    case QUUX_KIND_MAP: {
        bind_visitor visitor = {state};
        quux_map_for_each(value, bind_entry, &visitor);
        return;
    }
    default:
        return;
    }
}

static quux_value nth(quux_value list, uint64_t index) {
    for (uint64_t i = 0; i < index && !quux_is_nothing(list); i++) {
        list = quux_pair_rest(list);
    }
    return quux_is_nothing(list) ? quux_nothing : quux_pair_first(list);
}

static void collect_bindings(check_state *state, quux_value form);

static bool collect_entry(void *data, quux_value key, quux_value value) {
    bind_visitor *visitor = data;
    collect_bindings(visitor->state, key);
    collect_bindings(visitor->state, value);
    return true;
}

/* Collects the macros a file defines. */
static void collect_macros(check_state *state, quux_value form) {
    if (head_is(form, "define-macro") && quux_is_kind(nth(form, 1), QUUX_KIND_SYMBOL)) {
        set_add(&state->macros, quux_name_bytes(nth(form, 1)), quux_name_length(nth(form, 1)));
    }
}

/* Collects the names a file defines or binds. */
static void collect_bindings(check_state *state, quux_value form) {
    quux_kind kind = quux_kind_of(form);
    if (kind == QUUX_KIND_VECTOR) {
        for (uint64_t i = 0; i < quux_vector_count(form); i++) {
            collect_bindings(state, quux_vector_element(form, i));
        }
        return;
    }
    if (kind == QUUX_KIND_MAP) {
        bind_visitor visitor = {state};
        quux_map_for_each(form, collect_entry, &visitor);
        return;
    }
    if (kind != QUUX_KIND_PAIR) {
        return;
    }
    if (head_is(form, "quote")) {
        return;
    }
    quux_value head = quux_pair_first(form);
    if (quux_is_kind(head, QUUX_KIND_SYMBOL) &&
        set_contains(&state->macros, quux_name_bytes(head), quux_name_length(head))) {
        /* a call of a macro defined here: its symbol arguments may be names it binds */
        for (quux_value cell = quux_pair_rest(form); !quux_is_nothing(cell);
             cell = quux_pair_rest(cell)) {
            if (quux_is_kind(quux_pair_first(cell), QUUX_KIND_SYMBOL)) {
                bind_all_symbols(state, quux_pair_first(cell));
            }
        }
    }
    if (head_is(form, "namespace")) {
        bind_all_symbols(state, quux_pair_rest(form)); /* its name and imported namespaces */
    } else if (head_is(form, "define")) {
        bind_all_symbols(state, nth(form, 1));
    } else if (head_is(form, "define-function") || head_is(form, "define-macro")) {
        bind_all_symbols(state, nth(form, 1));
        bind_all_symbols(state, nth(form, 2));
    } else if (head_is(form, "function")) {
        bind_all_symbols(state, nth(form, 1));
    } else if (head_is(form, "let") || head_is(form, "let-parallel") ||
               head_is(form, "with-scope")) {
        quux_value bindings = nth(form, 1);
        if (quux_is_kind(bindings, QUUX_KIND_VECTOR)) {
            uint64_t count = quux_vector_count(bindings);
            for (uint64_t i = 0; i < count; i += head_is(form, "with-scope") ? 1 : 2) {
                bind_all_symbols(state, quux_vector_element(bindings, i));
            }
        }
    } else if (head_is(form, "let-functions")) {
        quux_value specifications = nth(form, 1);
        if (quux_is_kind(specifications, QUUX_KIND_VECTOR)) {
            for (uint64_t i = 0; i < quux_vector_count(specifications); i++) {
                quux_value specification = quux_vector_element(specifications, i);
                bind_all_symbols(state, nth(specification, 0));
                bind_all_symbols(state, nth(specification, 1));
            }
        }
    } else if (head_is(form, "catch")) {
        bind_all_symbols(state, nth(form, 2));
    }
    for (quux_value cell = form; !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
        collect_bindings(state, quux_pair_first(cell));
    }
}

static void check_name(check_state *state, quux_value name, size_t line) {
    const char *text = quux_name_bytes(name);
    size_t length = quux_name_length(name);
    bool is_keyword = quux_kind_of(name) == QUUX_KIND_KEYWORD;
    char keyword[512];
    if (is_keyword) {
        if (length + 1 >= sizeof keyword) {
            return;
        }
        keyword[0] = ':';
        memcpy(keyword + 1, text, length);
        text = keyword;
        length++;
    } else {
        size_t namespace_length = quux_symbol_namespace_length(name);
        if (namespace_length > 0) {
            if (namespace_length != 4 || memcmp(text, "core", 4) != 0) {
                return; /* a name qualified with a program's own namespace */
            }
            text += 5;
            length -= 5;
        }
        if (set_contains(&state->bound, text, length)) {
            return;
        }
    }
    if (!set_contains(state->approved, text, length)) {
        printf("%s:%zu: %.*s is not approved in docs/naming.md\n", state->file, line, (int)length,
               text);
        state->offending++;
    }
}

/* Checks the symbols of a form (outside quote) and the keywords used as error kinds. */
static void check_form(check_state *state, quux_value form, size_t line);

typedef struct check_visitor {
    check_state *state;
    size_t line;
} check_visitor;

static bool check_entry(void *data, quux_value key, quux_value value) {
    check_visitor *visitor = data;
    check_form(visitor->state, key, visitor->line);
    check_form(visitor->state, value, visitor->line);
    return true;
}

static void check_form(check_state *state, quux_value form, size_t line) {
    switch (quux_kind_of(form)) {
    case QUUX_KIND_SYMBOL:
        check_name(state, form, line);
        return;
    case QUUX_KIND_VECTOR:
        for (uint64_t i = 0; i < quux_vector_count(form); i++) {
            check_form(state, quux_vector_element(form, i), line);
        }
        return;
    case QUUX_KIND_MAP: {
        check_visitor visitor = {state, line};
        quux_map_for_each(form, check_entry, &visitor);
        return;
    }
    case QUUX_KIND_PAIR:
        if (head_is(form, "quote")) {
            return;
        }
        if ((head_is(form, "error") || head_is(form, "catch") || head_is(form, "raises")) &&
            quux_is_kind(nth(form, 1), QUUX_KIND_KEYWORD)) {
            check_name(state, nth(form, 1), line);
        }
        for (quux_value cell = form; !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
            check_form(state, quux_pair_first(cell), line);
        }
        return;
    default:
        return;
    }
}

enum { PASS_MACROS, PASS_BINDINGS, PASS_CHECK };

/* Whether the rest of the line after a form says the form raises :undefined-name (an example
 * that uses an undefined name on purpose). */
static bool expects_undefined_name(const char *text, size_t length, size_t position) {
    size_t end = position;
    while (end < length && text[end] != '\n') {
        end++;
    }
    const char *marker = ";!! :undefined-name";
    size_t marker_length = strlen(marker);
    for (size_t i = position; i + marker_length <= end; i++) {
        if (memcmp(text + i, marker, marker_length) == 0) {
            return true;
        }
    }
    return false;
}

/* One pass over the forms of a text. */
static void check_text(check_state *state, const char *text, size_t length, size_t first_line,
                       int pass) {
    quux_context *context = state->context;
    quux_value form = quux_nothing;
    QUUX_ROOT_FRAME(context, &form);
    quux_reader reader;
    quux_reader_initialize(&reader, text, length);
    for (;;) {
        quux_reader_skip_blank(&reader);
        size_t line = first_line + reader.line - 1;
        quux_read_status status = quux_reader_read(context, &reader, &form);
        if (status != QUUX_READ_FORM) {
            if (status == QUUX_READ_ERROR || status == QUUX_READ_INCOMPLETE) {
                if (pass == PASS_CHECK) {
                    printf("%s:%zu: cannot read this form\n", state->file, line);
                    state->offending++;
                }
                context->pending_error = quux_nothing;
            }
            break;
        }
        if (pass == PASS_MACROS) {
            collect_macros(state, form);
        } else if (pass == PASS_BINDINGS) {
            collect_bindings(state, form);
        } else if (!expects_undefined_name(text, length, reader.position)) {
            check_form(state, form, line);
        }
    }
    QUUX_ROOT_FRAME_END(context);
}

static void check_markdown(check_state *state, const char *text, size_t length, int pass) {
    size_t line_number = 1;
    const char *position = text;
    const char *block_start = NULL;
    size_t block_line = 0;
    while (position < text + length) {
        const char *end = memchr(position, '\n', (size_t)(text + length - position));
        if (end == NULL) {
            end = text + length;
        }
        if (block_start == NULL && end - position >= 7 && memcmp(position, "```quux", 7) == 0) {
            block_start = end + 1;
            block_line = line_number + 1;
        } else if (block_start != NULL && end - position >= 3 && memcmp(position, "```", 3) == 0) {
            check_text(state, block_start, (size_t)(position - block_start), block_line, pass);
            block_start = NULL;
        }
        position = end + 1;
        line_number++;
    }
}

static bool ends_with(const char *text, const char *suffix) {
    size_t length = strlen(text);
    size_t suffix_length = strlen(suffix);
    return length >= suffix_length && strcmp(text + length - suffix_length, suffix) == 0;
}

typedef void (*file_visitor)(void *data, const char *path);

static void walk(const char *path, file_visitor visitor, void *data) {
    struct stat information;
    if (stat(path, &information) != 0) {
        return;
    }
    if (!S_ISDIR(information.st_mode)) {
        visitor(data, path);
        return;
    }
    DIR *directory = opendir(path);
    if (directory == NULL) {
        return;
    }
    char *names[1024];
    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL && count < 1024) {
        if (entry->d_name[0] != '.') {
            names[count++] = strdup(entry->d_name);
        }
    }
    closedir(directory);
    for (size_t i = 0; i < count; i++) {
        char full[4096];
        snprintf(full, sizeof full, "%s/%s", path, names[i]);
        walk(full, visitor, data);
        free(names[i]);
    }
}

/* The files checked together: a Markdown file alone, or every .qx file of one program
 * directory (expected.qx and actors.qx use names that main.qx defines). */
typedef struct file_group {
    char *paths[16];
    size_t count;
} file_group;

static void reset_names(check_state *state) {
    set_release(&state->bound);
    set_release(&state->macros);
    state->bound = (name_set){NULL, 0, 0};
    state->macros = (name_set){NULL, 0, 0};
}

static void check_group(check_state *state, const file_group *group) {
    reset_names(state);
    for (int pass = PASS_MACROS; pass <= PASS_CHECK; pass++) {
        for (size_t i = 0; i < group->count; i++) {
            size_t length = 0;
            char *text = quux_session_read_file(group->paths[i], &length);
            if (text == NULL) {
                continue;
            }
            state->file = group->paths[i];
            if (ends_with(group->paths[i], ".md")) {
                check_markdown(state, text, length, pass);
            } else {
                check_text(state, text, length, 1, pass);
            }
            free(text);
        }
    }
}

static void check_directory(check_state *state, const char *path) {
    DIR *directory = opendir(path);
    if (directory == NULL) {
        return;
    }
    char *names[1024];
    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL && count < 1024) {
        if (entry->d_name[0] != '.') {
            names[count++] = strdup(entry->d_name);
        }
    }
    closedir(directory);
    file_group program = {{NULL}, 0};
    for (size_t i = 0; i < count; i++) {
        char full[4096];
        snprintf(full, sizeof full, "%s/%s", path, names[i]);
        struct stat information;
        if (stat(full, &information) == 0 && S_ISDIR(information.st_mode)) {
            check_directory(state, full);
        } else if (ends_with(full, ".md") && strstr(full, "docs/spec") != NULL &&
                   strstr(full, "_section-template") == NULL) {
            file_group single = {{strdup(full)}, 1};
            check_group(state, &single);
            free(single.paths[0]);
        } else if (ends_with(full, ".qx") && program.count < 16) {
            program.paths[program.count++] = strdup(full);
        }
        free(names[i]);
    }
    if (program.count > 0) {
        check_group(state, &program);
    }
    for (size_t i = 0; i < program.count; i++) {
        free(program.paths[i]);
    }
}

typedef struct quux_check_request {
    name_set *approved;
    int offending;
} quux_check_request;

static int check_quux_names(quux_context *context, void *data) {
    quux_check_request *request = data;
    check_state state = {context, request->approved, {NULL, 0, 0}, {NULL, 0, 0}, 0, NULL};
    check_directory(&state, "programs");
    check_directory(&state, "docs/spec");
    reset_names(&state);
    request->offending = state.offending;
    return 0;
}

/* ---- C identifiers ---- */

typedef struct c_state {
    name_set *banned;
    int offending;
} c_state;

static void check_identifier(c_state *state, const char *path, size_t line, const char *word,
                             size_t length) {
    size_t start = 0;
    for (size_t i = 0; i <= length; i++) {
        if (i == length || word[i] == '_') {
            size_t part = i - start;
            char lower[128];
            if (part > 0 && part < sizeof lower) {
                for (size_t j = 0; j < part; j++) {
                    lower[j] = (char)tolower((unsigned char)word[start + j]);
                }
                if (set_contains(state->banned, lower, part)) {
                    printf("%s:%zu: %.*s contains the abbreviation \"%.*s\" (docs/naming.md H)\n",
                           path, line, (int)length, word, (int)part, lower);
                    state->offending++;
                }
            }
            start = i + 1;
        }
    }
}

static void check_c_file(void *data, const char *path) {
    c_state *state = data;
    if (!ends_with(path, ".c") && !ends_with(path, ".h")) {
        return;
    }
    size_t length = 0;
    char *text = quux_session_read_file(path, &length);
    if (text == NULL) {
        return;
    }
    size_t line = 1;
    for (size_t i = 0; i < length;) {
        char character = text[i];
        if (character == '\n') {
            line++;
            i++;
        } else if (character == '/' && i + 1 < length && text[i + 1] == '/') {
            while (i < length && text[i] != '\n') {
                i++;
            }
        } else if (character == '/' && i + 1 < length && text[i + 1] == '*') {
            i += 2;
            while (i + 1 < length && !(text[i] == '*' && text[i + 1] == '/')) {
                line += text[i] == '\n';
                i++;
            }
            i += 2;
        } else if (character == '"' || character == '\'') {
            char quote = character;
            i++;
            while (i < length && text[i] != quote) {
                i += text[i] == '\\' ? 2 : 1;
            }
            i++;
        } else if (isalpha((unsigned char)character) || character == '_') {
            size_t start = i;
            while (i < length && (isalnum((unsigned char)text[i]) || text[i] == '_')) {
                i++;
            }
            bool reserved = i - start > 2 && text[start] == '_' && text[start + 1] == '_';
            if (!reserved) { /* __VA_ARGS__ and other names of the C implementation */
                check_identifier(state, path, line, text + start, i - start);
            }
        } else {
            i++;
        }
    }
    free(text);
}

int main(int argument_count, char **arguments) {
    name_set approved = {NULL, 0, 0};
    name_set banned = {NULL, 0, 0};
    if (!load_glossary("docs/naming.md", &approved, &banned)) {
        return 2;
    }
    int offending = 0;
    if (argument_count >= 2 && strcmp(arguments[1], "--c") == 0) {
        c_state state = {&banned, 0};
        for (int i = 2; i < argument_count; i++) {
            walk(arguments[i], check_c_file, &state);
        }
        offending = state.offending;
    } else if (argument_count == 1) {
        quux_check_request request = {&approved, 0};
        int status = quux_session_run(check_quux_names, &request);
        if (status != 0) {
            return status;
        }
        offending = request.offending;
    } else {
        fprintf(stderr, "usage: quux-name-check [--c DIRECTORY ...]\n");
        return 2;
    }
    set_release(&approved);
    set_release(&banned);
    if (offending > 0) {
        printf("quux-name-check: %d offending name%s\n", offending, offending == 1 ? "" : "s");
        return 1;
    }
    printf("quux-name-check: all names approved\n");
    return 0;
}
