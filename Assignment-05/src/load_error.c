#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "load_error.h"

jmp_buf g_load_error_jmp;
char    g_load_error_msg[256];

_Noreturn void load_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_load_error_msg, sizeof(g_load_error_msg), fmt, ap);
    va_end(ap);
    /* Several of these format strings came from converted fprintf(stderr, ...)
     * calls that included their own trailing '\n'; strip it so callers that
     * add their own punctuation/newline (like os.c) don't get a stray blank
     * line or an orphaned line break in the middle of a sentence. */
    size_t len = strlen(g_load_error_msg);
    while (len > 0 && (g_load_error_msg[len - 1] == '\n' || g_load_error_msg[len - 1] == '\r')) {
        g_load_error_msg[--len] = '\0';
    }
    fprintf(stderr, "Error: %s\n", g_load_error_msg);
    longjmp(g_load_error_jmp, 1);
}
