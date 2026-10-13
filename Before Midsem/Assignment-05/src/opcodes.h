#ifndef OPCODES_H
#define OPCODES_H


#define OP_HALT              0x00
#define OP_PRINT             0x08

#define OP_ADD_VAR           0x01
#define OP_SUB_VAR           0x02
#define OP_MUL_VAR           0x03
#define OP_DIV_VAR           0x04
#define OP_MEMREAD_VAR       0x05
#define OP_MEMWRITE_VAR      0x06
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

#define NUM_PROCESSORS 4

#define MEMSIZE              8192
#define PAGESIZE             512
#define NUM_PHYSICAL_PAGES   (MEMSIZE / PAGESIZE)              /* 16 */
#define INSTR_LOGICAL_SIZE   1024                              /* 256 instructions */
#define DATA_LOGICAL_SIZE    4096
#define NUM_INSTR_PAGES      (INSTR_LOGICAL_SIZE / PAGESIZE)   /* 2  */
#define NUM_DATA_PAGES       (DATA_LOGICAL_SIZE / PAGESIZE)    /* 8  */
#define NUM_LOGICAL_PAGES    (NUM_INSTR_PAGES + NUM_DATA_PAGES)/* 10 */
#define MAX_PROC             NUM_PROCESSORS

// #define PT_BASE_FRAME   1                         /* frames 1..MAX_PROC hold page tables */
// #define FIRST_FREE_FRAME (PT_BASE_FRAME + MAX_PROC)

#endif
