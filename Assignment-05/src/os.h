#ifndef OS_H
#define OS_H

#define MAX_TASKS      64   /* max tasks (ready+waiting) the OS tracks over its lifetime */
#define TIME_SLICE     10   /* instructions each ready task gets per scheduler round */
#define MAX_PATH_LEN   128

/* Sets up terminal (raw, non-blocking stdin) and internal task tables.
 * argv[1]/argv[2], if given, are loaded as an initial task (program
 * [data]) before the interactive loop starts - handy for
 * non-interactive/scripted testing, on top of the shell's own
 * "type a program name" flow described in the handout. */
void os_init(int argc, char **argv);

/* One round: gives every ready task a time slice via
 * process_instructions(), reaps any that finished, promotes queued
 * tasks onto freed processors, then services the shell. */
void os_scheduler(void);

/* Non-blocking-read one shell "tick": consumes whatever bytes are
 * currently waiting on stdin, echoes/back-spaces them, and once a full
 * line (Enter) has been typed, hands it to the loader. Typing "exit"
 * stops accepting *new* tasks (already-running tasks still finish). */
void os_shell(void);

/* Compiles prog_src (assembly or already-compiled program.byte) into a
 * private per-task program.byte and assigns it to a free processor; if
 * none is free, the task is queued and picked up automatically once a
 * processor frees up. data_src may be NULL. */
void os_loader(const char *prog_src, const char *data_src);

/* Runs os_scheduler() in a loop until "exit" has been typed *and* every
 * task (ready + waiting) has finished, then restores the terminal and
 * returns. */
void os_run(void);

#endif
