#ifndef COMPAT_H
#define COMPAT_H
#include <stddef.h>

/* strcasecmp()/strncasecmp() are POSIX, not standard C - MinGW on
 * Windows doesn't reliably provide them (no <strings.h> at all in a
 * plain MinGW toolchain). These do the same job without depending on
 * any platform-specific header, so the same source builds unchanged
 * on Linux and Windows. */
int  ci_strcasecmp(const char *a, const char *b);
int  ci_strncasecmp(const char *a, const char *b, size_t n);

#endif
