#ifndef LOAD_ERROR_H
#define LOAD_ERROR_H
#include <setjmp.h>

extern jmp_buf g_load_error_jmp;
extern char    g_load_error_msg[256];

_Noreturn void load_error(const char *fmt, ...);

#endif
