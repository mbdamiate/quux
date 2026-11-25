#include "quux_unit_evaluation.h"

#include "quux_printer.h"

#include <string.h>

bool quux_unit_evaluates_to(quux_context *context, const char *text, const char *expected) {
    quux_value value = quux_nothing;
    QUUX_ROOT_FRAME(context, &value);
    bool evaluated = quux_evaluate_text(context, text, strlen(text), &value);
    quux_text printed;
    quux_text_initialize(&printed);
    if (evaluated) {
        quux_print(context, &printed, value, QUUX_PRINT_READABLE);
    } else {
        quux_text_append_c(&printed, "!! ");
        quux_value error = context->pending_error;
        if (quux_is_kind(error, QUUX_KIND_ERROR)) {
            quux_print(context, &printed, quux_error_field(error, QUUX_ERROR_KIND),
                       QUUX_PRINT_READABLE);
        }
        context->pending_error = quux_nothing;
    }
    context->recursion_depth = 0;
    bool matches = printed.bytes != NULL && strcmp(printed.bytes, expected) == 0;
    if (!matches) {
        fprintf(stderr, "  evaluating %s\n    expected %s\n    got      %s\n", text, expected,
                printed.bytes != NULL ? printed.bytes : "(nothing printed)");
    }
    quux_text_release(&printed);
    QUUX_ROOT_FRAME_END(context);
    return matches;
}
