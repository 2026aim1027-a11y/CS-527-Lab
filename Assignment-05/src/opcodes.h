#ifndef OPCODES_H
#define OPCODES_H

/* ---------------------------------------------------------------------
 * Bytecode opcode map.
 *
 * This follows the assignment's table wherever it is unambiguous, and
 * is verified against the single worked example in the handout
 * (the array-sum program and its bytecode dump): every opcode below
 * that also appears in that dump was cross-checked against it.
 *
 * Two problems in the handout's table had to be resolved by hand:
 *
 *  1. "Divide" (const form) and "Integer Memory read" (const form) are
 *     both listed as 0x0C. That's a straight collision. 0x0D is unused
 *     everywhere else in the table, so Integer-Memory-read-with-
 *     constant-address is moved there.
 *
 *  2. Vector add/sub/multiply only have two bytecode forms listed
 *     (var, const), but the language spec shows THREE distinct source
 *     forms: vector-op-vector ("v3 = v1 + v4"), vector-op-scalar-register
 *     ("v3 = v1 * x4"), and vector-op-constant ("v3 = v1 + 15"). The
 *     table's "var" opcode can't mean both "vector register" and
 *     "integer register" at once, because the processor has no other
 *     way to tell a v-register index from an x-register index apart
 *     (both are plain 0-255 byte fields). I added a third opcode band
 *     (0x31-0x33) for the vector-op-scalar-register form; 0x21-0x23
 *     stays vector-op-vector, 0x29-0x2B stays vector-op-constant.
 * --------------------------------------------------------------------- */

#define OP_HALT              0x00

/* Lab 4: Print <xreg> -> logs "Process id: <pid>  x<reg> : <hex value>"
 * to the OS-wide log file. dest = 0, src1 = 0, src2 = register number,
 * exactly as the handout specifies ("Dest register as well as operand 1
 * is '0', register mentioned in Print instruction will be at operand 2"). */
#define OP_PRINT             0x08

#define OP_ADD_VAR           0x01
#define OP_SUB_VAR           0x02
#define OP_MUL_VAR           0x03
#define OP_DIV_VAR           0x04
#define OP_MEMREAD_VAR       0x05
#define OP_MEMWRITE_VAR      0x06
/* 0x07 unused: data-movement only ever has a constant source in the
 * language spec ("<Dest> = <constant value>"); a plain register-to-
 * register copy is compiled as "add <src>, 0" instead (see compiler.c). */
#define OP_ADD_CONST         0x09
#define OP_SUB_CONST         0x0A
#define OP_MUL_CONST         0x0B
#define OP_DIV_CONST         0x0C
#define OP_MEMREAD_CONST     0x0D  /* moved from the colliding 0x0C, see note above */
#define OP_MEMWRITE_CONST    0x0E
#define OP_DATAMOVE_CONST    0x0F

#define OP_BRANCH_BASE       0x10  /* + 4-bit condition code, so 0x10-0x1E */

#define OP_VADD_VEC          0x21
#define OP_VSUB_VEC          0x22
#define OP_VMUL_VEC          0x23
#define OP_VMEMREAD_VAR      0x25
#define OP_VMEMWRITE_VAR     0x26
#define OP_VADD_CONST        0x29
#define OP_VSUB_CONST        0x2A
#define OP_VMUL_CONST        0x2B
#define OP_VMEMREAD_CONST    0x2C
#define OP_VMEMWRITE_CONST   0x2E

#define OP_VADD_SCALARREG    0x31  /* v = v OP x-scalar-register (added, not in handout table, see note above) */
#define OP_VSUB_SCALARREG    0x32
#define OP_VMUL_SCALARREG    0x33

/* Branch condition codes (low nibble of a branch opcode) */
#define BC_EQ 0x0
#define BC_NE 0x1
#define BC_CS 0x2
#define BC_CC 0x3
#define BC_MI 0x4
#define BC_PL 0x5
#define BC_VS 0x6
#define BC_VC 0x7
#define BC_HI 0x8
#define BC_LS 0x9
#define BC_GE 0xA
#define BC_LT 0xB
#define BC_GT 0xC
#define BC_LE 0xD
#define BC_AL 0xE

#define VEC_LEN     8   /* elements per 256-bit vector register (8 x 32-bit) */
#define NUM_INT_REG 256
#define NUM_VEC_REG 32

/* Lab 4: fixed number of physical processors the OS multiplexes tasks
 * across (the handout's diagram shows "Processor 0 .. Processor (NP-1)"
 * and its text mentions "all four processors" as the concrete example,
 * so NP=4 here). */
#define NUM_PROCESSORS 4

/* ---------------------------------------------------------------------
 * Lab 5: paged virtual memory.
 *
 * These are exactly the numbers the handout works through by hand
 * ("Number of pages: 1024/512 + 4096/512 = 10", "Number of physical
 * pages ... assuming memsize is 8192, it is 16"):
 *
 *   MEMSIZE=8192, PAGESIZE=512  -> 16 physical frames total, frame 0
 *   reserved and never allocated ("Frame 0 is reserved").
 *
 *   The handout's own page-table arithmetic implies a 1024-byte logical
 *   instruction space (1024/512 = 2 instruction pages), i.e. 256
 *   instructions of 4 bytes each - NOT the 256-*byte* / 64-instruction
 *   instruction memory Labs 1-4 actually used. This is a real
 *   inconsistency between this handout's own diagram (still labelled
 *   "Instruction memory Size 256 byte") and its arithmetic; the
 *   arithmetic is unambiguous and explicit, so it wins: instruction
 *   capacity goes from 64 to 256 instructions in Lab 5. This is a
 *   strict superset (every Lab 1-4 program still fits), so nothing
 *   breaks, but it IS a capacity change worth knowing about.
 *
 *   4096/512 = 8 data pages, matching every earlier lab's 4096-byte
 *   data memory - no change there.
 *
 *   MAX_PROC (the handout's own name for the page-table array's first
 *   dimension) = NUM_PROCESSORS: a page table lives with the physical
 *   processor slot, reset/rebuilt per task, exactly like Register[NP],
 *   PC[NP], etc. already do. */
#define MEMSIZE              8192
#define PAGESIZE             512
#define NUM_PHYSICAL_PAGES   (MEMSIZE / PAGESIZE)              /* 16 */
#define INSTR_LOGICAL_SIZE   1024                              /* 256 instructions */
#define DATA_LOGICAL_SIZE    4096
#define NUM_INSTR_PAGES      (INSTR_LOGICAL_SIZE / PAGESIZE)   /* 2  */
#define NUM_DATA_PAGES       (DATA_LOGICAL_SIZE / PAGESIZE)    /* 8  */
#define NUM_LOGICAL_PAGES    (NUM_INSTR_PAGES + NUM_DATA_PAGES)/* 10 */
#define MAX_PROC             NUM_PROCESSORS

#endif
