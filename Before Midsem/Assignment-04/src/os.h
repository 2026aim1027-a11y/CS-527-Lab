#ifndef OS_H
#define OS_H

#define MAX_TASKS      64   /* max tasks (ready+waiting) the OS tracks over its lifetime */
#define TIME_SLICE     10   /* instructions each ready task gets per scheduler round */
#define MAX_PATH_LEN   128

void os_init(int argc, char **argv);

void os_scheduler(void);
void os_shell(void);
void os_loader(const char *prog_src, const char *data_src);

void os_run(void);

#endif
