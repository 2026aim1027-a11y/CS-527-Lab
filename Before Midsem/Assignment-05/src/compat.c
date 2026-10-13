#include <ctype.h>
#include "compat.h"

int ci_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int ci_strncasecmp(const char *a, const char *b, size_t n) {
    while (n > 0 && *a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++; n--;
    }
    if (n == 0) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}
