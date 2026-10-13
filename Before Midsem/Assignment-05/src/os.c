#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "os.h"
#include "opcodes.h"
#include "compiler.h"
#include "memory.h"
#include "processor.h"
#include "load_error.h"
#include "compat.h"    /* ci_strcasecmp - portable, no <strings.h> dependency */

/* Non-blocking, character-at-a-time stdin access is inherently
 * platform-specific (there's no standard-C way to ask "is a byte
 * available on stdin without blocking?"). POSIX (Linux/macOS) uses
 * termios raw mode + O_NONBLOCK; Windows has no <termios.h> at all
 * (not even under MinGW), so it uses the console API / conio.h
 * instead. Both sides implement the same tiny interface:
 *   shell_setup_terminal() / shell_restore_terminal() - once, at
 *     startup/exit.
 *   shell_read_char(char *out) -> 1 if a byte was read into *out,
 *     0 if nothing is available *right now* (try again next tick),
 *     -1 on EOF/closed stream (treated like typing "exit").
 * os_shell() below is written entirely against this interface and
 * doesn't otherwise care which platform it's on. */
#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    #include <io.h>       /* _isatty, _fileno */

    static int    win_is_console = 0;
    static HANDLE win_stdin_handle;

    static void shell_restore_terminal(void) {
        if (win_is_console) {
            DWORD mode;
            if (GetConsoleMode(win_stdin_handle, &mode)) {
                SetConsoleMode(win_stdin_handle, mode | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
            }
        }
    }

    static void shell_setup_terminal(void) {
        win_stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
        win_is_console = _isatty(_fileno(stdin));
        if (win_is_console) {
            DWORD mode;
            if (GetConsoleMode(win_stdin_handle, &mode)) {
                atexit(shell_restore_terminal);
                /* turn off the console's own line-buffering and echo, so we
                 * can assemble/echo lines ourselves exactly like raw mode
                 * does on POSIX (see handle below) */
                SetConsoleMode(win_stdin_handle, mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
            }
        }
    }

    static int shell_read_char(char *out) {
        if (win_is_console) {
            if (_kbhit()) { *out = (char)_getch(); return 1; }
            return 0;
        }
        /* Redirected input (`sim < script.txt`, or a pipe): _kbhit()/_getch()
         * only work on a real console, so fall back to the Win32 file API.
         * PeekNamedPipe tells us whether a *pipe* has data waiting without
         * consuming it; it simply fails (harmlessly) on a plain redirected
         * file, and reading a regular file never blocks anyway, so falling
         * through to ReadFile is safe either way. */
        DWORD avail = 0;
        if (PeekNamedPipe(win_stdin_handle, NULL, 0, NULL, &avail, NULL) && avail == 0) {
            return 0;
        }
        DWORD readBytes = 0;
        char buf;
        if (!ReadFile(win_stdin_handle, &buf, 1, &readBytes, NULL) || readBytes == 0) {
            return -1; /* EOF / closed */
        }
        *out = buf;
        return 1;
    }
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <termios.h>

    static struct termios orig_termios;
    static int have_orig_termios = 0;

    static void shell_restore_terminal(void) {
        if (have_orig_termios) {
            tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        }
    }

    static void shell_setup_terminal(void) {
        if (tcgetattr(STDIN_FILENO, &orig_termios) == 0) {
            have_orig_termios = 1;
            atexit(shell_restore_terminal);
            struct termios raw = orig_termios;
            raw.c_lflag &= ~(ICANON | ECHO);
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        }
        /* If stdin isn't a real TTY (piped/redirected input), tcgetattr()
         * above simply fails and we skip raw mode - O_NONBLOCK below is
         * still enough to drain a piped script line by line without
         * blocking the scheduler. */
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (flags != -1) fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    static int shell_read_char(char *out) {
        ssize_t n = read(STDIN_FILENO, out, 1);
        if (n > 0) return 1;
        if (n == 0) return -1; /* EOF */
        return 0;              /* EAGAIN/EWOULDBLOCK: nothing available yet */
    }
#endif

/* ----------------------------------------------------------------------
 * Task bookkeeping.
 *
 * A "task" = one user program, identified by a PID assigned at load
 * time. A task is either:
 *   - RUNNING: owns one of the NUM_PROCESSORS processors, and appears
 *     in ready_pids[] so the scheduler gives it time slices, or
 *   - WAITING: every processor was busy when it was loaded, so it sits
 *     in waiting_queue[] until one frees up (handout: "if all the ...
 *     processors are busy executing, a new task is created, that will
 *     [be] kept in waiting queue").
 * ------------------------------------------------------------------- */

typedef struct {
    int  pid;
    char data_out[MAX_PATH_LEN]; /* where to write this task's data.byte when it finishes */
    int  has_data_out;
} RunningTask;

typedef struct {
    int  pid;
    char prog[MAX_PATH_LEN];
    char data[MAX_PATH_LEN];
    int  has_data;
} PendingTask;

static int proc_busy[NUM_PROCESSORS];      /* is processor p currently owned by a task? */
static int pid_to_proc[MAX_TASKS];         /* -1 if not currently mapped to a processor */

static RunningTask ready[MAX_TASKS];
static int         ready_count = 0;

static PendingTask waiting_queue[MAX_TASKS];
static int         waiting_count = 0;

static int next_pid = 1;
static int exit_requested = 0; /* shell got "exit": stop accepting *new* tasks */

/* ---------- small helpers ------------------------------------------- */

static void trim_inplace(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
    int start = 0;
    while (s[start] && isspace((unsigned char)s[start])) start++;
    if (start > 0) memmove(s, s + start, strlen(s + start) + 1);
}

static int find_free_processor(void) {
    for (int p = 0; p < NUM_PROCESSORS; p++)
        if (!proc_busy[p]) return p;
    return -1;
}

static void remove_ready_at(int idx) {
    ready[idx] = ready[ready_count - 1];
    ready_count--;
}

/* Actually binds an already-PID'd task onto a specific free processor:
 * compiles its source, loads it into that processor's memory, resets
 * the processor, and adds it to the ready queue. Shared by os_loader()
 * (fresh task, processor free right now) and the scheduler's
 * queue-promotion step (a task that had been waiting).
 *
 * compile()/mem_initialize() can fail on bad input (missing file,
 * syntax error, undefined label, ...); those failures call
 * load_error(), which longjmps back to the setjmp() right here instead
 * of exit()ing the whole OS - see load_error.h for why. Returns 1 on
 * success, 0 on failure (processor p is left free either way in that
 * case, and no ready-queue entry is created). */
static int assign_task_to_processor(int pid, const char *prog_src, const char *data_src, int p) {
    char program_byte[MAX_PATH_LEN];
    snprintf(program_byte, sizeof(program_byte), "task_%d_program.byte", pid);

    if (setjmp(g_load_error_jmp) != 0) {
        printf("[OS] Failed to load task %d ('%s') onto processor %d: %s "
               "(task discarded, processor %d still free).\n",
               pid, prog_src, p, g_load_error_msg, p);
        return 0;
    }

    compile(prog_src, program_byte);
    mem_initialize(p, program_byte, data_src);
    reset(p);

    proc_busy[p] = 1;
    pid_to_proc[pid] = p;

    RunningTask *t = &ready[ready_count++];
    t->pid = pid;
    if (data_src) {
        t->has_data_out = 1;
        strncpy(t->data_out, data_src, sizeof(t->data_out) - 1);
        t->data_out[sizeof(t->data_out) - 1] = '\0';
    } else {
        t->has_data_out = 1;
        snprintf(t->data_out, sizeof(t->data_out), "task_%d_data.byte", pid);
    }

    printf("[OS] Loaded task %d ('%s') onto processor %d.\n", pid, prog_src, p);
    return 1;
}

void os_loader(const char *prog_src, const char *data_src) {
    if (ready_count >= MAX_TASKS || next_pid >= MAX_TASKS) {
        fprintf(stderr, "[OS] Task table full, dropping '%s'\n", prog_src);
        return;
    }

    int pid = next_pid++;
    int p = find_free_processor();

    if (p >= 0) {
        assign_task_to_processor(pid, prog_src, data_src, p);
    } else {
        PendingTask *t = &waiting_queue[waiting_count++];
        t->pid = pid;
        strncpy(t->prog, prog_src, sizeof(t->prog) - 1); t->prog[sizeof(t->prog) - 1] = '\0';
        if (data_src) {
            t->has_data = 1;
            strncpy(t->data, data_src, sizeof(t->data) - 1); t->data[sizeof(t->data) - 1] = '\0';
        } else {
            t->has_data = 0;
        }
        printf("[OS] All %d processors busy - task %d ('%s') queued.\n", NUM_PROCESSORS, pid, prog_src);
    }
}

static void promote_from_waiting_queue(int freed_proc) {
    /* Try queued tasks in FIFO order until one actually loads; a bad
     * program file sitting further back in the queue shouldn't leave
     * this processor idle when a later, valid task could use it. */
    while (waiting_count > 0) {
        PendingTask t = waiting_queue[0];
        for (int i = 1; i < waiting_count; i++) waiting_queue[i - 1] = waiting_queue[i];
        waiting_count--;
        if (assign_task_to_processor(t.pid, t.prog, t.has_data ? t.data : NULL, freed_proc)) {
            return;
        }
        /* that one failed to load; loop around and try the next, if any */
    }
}

/* ---------- scheduler ------------------------------------------------ */

void os_scheduler(void) {
    /* "for(all the active processes, pid) { proc_id = map_pid_proc_id(pid);
     *    process_instructions(pid, 10); }" - round robin, one time slice each. */
    for (int i = 0; i < ready_count; /* advance handled below */) {
        int pid = ready[i].pid;
        int p = pid_to_proc[pid];

        process_instructions(p, TIME_SLICE);

        if (end_of_simulation[p]) {
            mem_finalize(p, ready[i].has_data_out ? ready[i].data_out : NULL);
            if (task_fault[p]) {
                printf("[OS] Task %d FAULTED on processor %d: %s (data -> %s; other tasks unaffected).\n",
                       pid, p, task_fault_msg[p], ready[i].data_out);
            } else {
                printf("[OS] Task %d finished on processor %d (data -> %s).\n",
                       pid, p, ready[i].data_out);
            }
            pid_to_proc[pid] = -1;
            proc_busy[p] = 0;
            remove_ready_at(i);        /* swap-remove: don't advance i, re-check this slot */
            promote_from_waiting_queue(p);
        } else {
            i++;
        }
    }

    os_shell();
}

/* ---------- shell ------------------------------------------------------
 * Non-blocking, character-at-a-time stdin reader. Because raw mode
 * turns off the kernel's own echo, we echo printable characters and
 * handle backspace ourselves; a full line is handed to the loader (or
 * treated as "exit") the moment Enter/EOF is seen.
 * --------------------------------------------------------------------- */

static char   line_buf[256];
static int    line_len = 0;
static int    prompt_shown = 0;

static void handle_shell_line(char *line) {
    trim_inplace(line);
    if (line[0] == '\0') return;

    if (ci_strcasecmp(line, "exit") == 0) {
        exit_requested = 1;
        printf("[OS] 'exit' received - no new tasks will be accepted; "
               "running/queued tasks will still finish.\n");
        return;
    }

    /* "<program file> [data file]" - data file is optional */
    char prog[MAX_PATH_LEN] = {0}, data[MAX_PATH_LEN] = {0};
    int n = sscanf(line, "%127s %127s", prog, data);
    if (n < 1) return;
    os_loader(prog, (n == 2) ? data : NULL);
}

void os_shell(void) {
    if (exit_requested) return; /* stop taking new input, per spec */

    if (!prompt_shown) {
        printf("$ ");
        fflush(stdout);
        prompt_shown = 1;
    }

    char c;
    int r;
    while ((r = shell_read_char(&c)) == 1) {
        if (c == '\n' || c == '\r') {
            line_buf[line_len] = '\0';
            printf("\n");
            handle_shell_line(line_buf);
            line_len = 0;
            if (!exit_requested) { printf("$ "); fflush(stdout); }
            else return;
        } else if (c == 127 || c == 8) { /* backspace/DEL */
            if (line_len > 0) { line_len--; printf("\b \b"); fflush(stdout); }
        } else if (line_len < (int)sizeof(line_buf) - 1) {
            line_buf[line_len++] = c;
            putchar(c);
            fflush(stdout);
        }
    }
    if (r == -1) {
        /* EOF on stdin (e.g. piped input script ended): behave like "exit" */
        printf("\n[OS] EOF on stdin - treating as 'exit'.\n");
        exit_requested = 1;
    }
}

/* ---------- lifecycle -------------------------------------------------- */

void os_init(int argc, char **argv) {
    for (int p = 0; p < NUM_PROCESSORS; p++) proc_busy[p] = 0;
    for (int i = 0; i < MAX_TASKS; i++) pid_to_proc[i] = -1;
    ready_count = 0;
    waiting_count = 0;
    next_pid = 1;
    exit_requested = 0;
    line_len = 0;
    prompt_shown = 0;

    shell_setup_terminal();
    mmu_init_global(); /* Lab 5: mark frame 0 reserved, every other physical frame free */

    printf("[OS] CS527 Lab 5 multi-processing, paged-memory OS - %d processors, "
           "%d physical frames (%d bytes each, frame 0 reserved).\n",
           NUM_PROCESSORS, NUM_PHYSICAL_PAGES - 1, PAGESIZE);
    printf("[OS] Type a program file (optionally followed by a data file) at the\n"
           "     '$' prompt to load a new task, or 'exit' to stop accepting new ones.\n");

    /* Optional convenience for scripted/automated testing: `./sim prog [data]`
     * pre-loads one task immediately, on top of the interactive shell. */
    if (argc >= 2) {
        os_loader(argv[1], (argc >= 3) ? argv[2] : NULL);
    }
}

void os_run(void) {
    while (!exit_requested || ready_count > 0 || waiting_count > 0) {
        os_scheduler();
    }
    shell_restore_terminal();
    printf("[OS] All tasks complete. Shutting down.\n");
}
