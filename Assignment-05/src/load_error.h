#ifndef LOAD_ERROR_H
#define LOAD_ERROR_H
#include <setjmp.h>

/* Lab 4 hardening: compile()/mem_initialize() used to call exit(1) on
 * any bad input (missing file, syntax error, undefined label, an
 * out-of-range constant, ...). That was fine for Labs 1-3, where main()
 * ran exactly one program per invocation - but Lab 4's shell can be
 * asked to load a new task at any time, and a typo in a filename or a
 * bug in a *newly submitted* program must not kill every other task
 * already running on the OS.
 *
 * os_loader() wraps each load attempt in setjmp(g_load_error_jmp); any
 * exit(1)-turned-load_error() call inside compile()/mem_initialize()
 * longjmps back there instead of terminating the process, so the OS
 * simply rejects that one task and keeps running everything else.
 * This is unrelated to report_runtime_fault() in processor.h, which
 * handles errors *during execution* of an already-loaded task (those
 * happen deep inside the scheduler's call stack, long after any
 * setjmp() here has gone out of scope, so they must not longjmp here). */
extern jmp_buf g_load_error_jmp;
extern char    g_load_error_msg[256];

/* Formats msg (printf-style) into g_load_error_msg, prints it to
 * stderr, and longjmps to g_load_error_jmp. Marked _Noreturn since it
 * genuinely never returns - several call sites are the last statement
 * in a non-void function (e.g. find_label()), and without this the
 * compiler can't see that and warns about falling off the end. */
_Noreturn void load_error(const char *fmt, ...);

#endif
