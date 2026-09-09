#ifndef PROCESSOR_H
#define PROCESSOR_H
#include "opcodes.h"

/* Lab 4: Register[NP][256] / PC[NP] / end_of_simulation[NP], exactly as
 * the handout's pseudocode lists them. opcode/dest/src1/src2 stay plain
 * (non-arrayed) scalars: they're just the scratch decode of "whichever
 * processor is currently being stepped", valid only for the duration of
 * one fetch->decode->execute, matching the handout listing them without
 * a [NP] subscript while PC/Register/end_of_simulation get one. */
extern int Register[NUM_PROCESSORS][NUM_INT_REG];
extern int VecRegister[NUM_PROCESSORS][NUM_VEC_REG][VEC_LEN];

extern int PC[NUM_PROCESSORS];
extern int opcode, dest, src1, src2;
extern int flagZ[NUM_PROCESSORS], flagN[NUM_PROCESSORS], flagC[NUM_PROCESSORS], flagV[NUM_PROCESSORS];
extern int end_of_simulation[NUM_PROCESSORS];

/* Lab 4 hardening: a single task's runtime error (divide by zero, an
 * out-of-bounds memory access, a corrupt/unknown opcode) must not take
 * the whole multi-tasking OS down with it - only that task should stop.
 * task_fault[p] is set the first time processor p's current task hits
 * one of these; task_fault_msg[p] holds a short description. Cleared by
 * reset(p) when a processor is (re)assigned to a new task. */
extern int  task_fault[NUM_PROCESSORS];
extern char task_fault_msg[NUM_PROCESSORS][128];

/* Records a runtime fault on processor p: sets task_fault[p],
 * end_of_simulation[p] (so process_instructions() stops stepping it
 * immediately), and logs it - called from processor.c's own execute()
 * (divide-by-zero, unknown opcode) and from memory.c (out-of-bounds
 * read/write), instead of either calling exit(). */
void report_runtime_fault(int proc_id, const char *msg);

/* "Int proc_id" from the handout: which processor fetch/decode/execute
 * currently operate on. Set this before calling fetch(). */
extern int proc_id;

void reset(int p);      /* zero processor p's state; lazily opens the shared log */
void fetch(void);       /* fetch()/decode()/execute() all act on `proc_id` */
void decode(void);
void execute(void);

/* Runs up to instruction_count fetch/decode/execute cycles on processor
 * p (stopping early if it halts), then sleeps ~10us - this is the
 * "time slice" the OS scheduler hands each ready task. */
void process_instructions(int p, int instruction_count);

#endif
