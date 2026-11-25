/* quux-test: the test tool (D-46, contracts/command-line.md).
 *   quux-test unit [MODULE]       C unit tests
 *   quux-test examples PATH       specification examples (;=> and ;!!)
 *   quux-test program DIRECTORY   one reference program (from User Story 3) */
#include "quux_harness.h"
#include "quux_unit_test.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr, "usage: quux-test unit [MODULE]\n"
                    "       quux-test [--verify-roots | --verify-roots-every-allocation]\n"
                    "                 [--available FEATURES] examples PATH\n"
                    "       quux-test program DIRECTORY\n");
    return 2;
}

static bool ends_with(const char *text, const char *suffix) {
    size_t length = strlen(text);
    size_t suffix_length = strlen(suffix);
    return length >= suffix_length && strcmp(text + length - suffix_length, suffix) == 0;
}

static int compare_names(const void *left, const void *right) {
    return strcmp(*(char *const *)left, *(char *const *)right);
}

/* A file, or every .md file of a directory except the section template. */
static int run_examples_path(const char *path, const quux_example_options *options) {
    if (ends_with(path, ".md")) {
        return quux_run_examples_file(path, options) == 0 ? 0 : 1;
    }
    DIR *directory = opendir(path);
    if (directory == NULL) {
        fprintf(stderr, "quux-test: cannot open %s\n", path);
        return 2;
    }
    char *names[256];
    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL && count < 256) {
        if (ends_with(entry->d_name, ".md") && entry->d_name[0] != '_') {
            names[count++] = strdup(entry->d_name);
        }
    }
    closedir(directory);
    qsort(names, count, sizeof names[0], compare_names);
    int failures = 0;
    for (size_t i = 0; i < count; i++) {
        char full[4096];
        snprintf(full, sizeof full, "%s/%s", path, names[i]);
        failures += quux_run_examples_file(full, options) == 0 ? 0 : 1;
        free(names[i]);
    }
    return failures == 0 ? 0 : 1;
}

int main(int argument_count, char **arguments) {
    quux_example_options options = {"core", false, false};
    while (argument_count > 1 && strncmp(arguments[1], "--", 2) == 0) {
        if (strcmp(arguments[1], "--verify-roots-every-allocation") == 0) {
            options.verify_every_allocation = true;
            arguments++;
            argument_count--;
        } else if (strcmp(arguments[1], "--verify-roots") == 0) {
            options.verify_roots = true;
            arguments++;
            argument_count--;
        } else if (strcmp(arguments[1], "--available") == 0 && argument_count > 2) {
            options.available_features = arguments[2];
            arguments += 2;
            argument_count -= 2;
        } else {
            return usage();
        }
    }
    if (argument_count < 2) {
        return usage();
    }
    if (strcmp(arguments[1], "examples") == 0 && argument_count == 3) {
        return run_examples_path(arguments[2], &options);
    }
    if (strcmp(arguments[1], "unit") == 0) {
        int failures = quux_unit_run(argument_count > 2 ? arguments[2] : NULL);
        if (failures < 0) {
            fprintf(stderr, "quux-test: no unit test module named %s\n", arguments[2]);
            return 2;
        }
        return failures == 0 ? 0 : 1;
    }
    return usage();
}
