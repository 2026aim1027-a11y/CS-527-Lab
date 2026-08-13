#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "compiler.h"

/* ==================================================================== */
/* Branch condition suffix table (matches the CS527 spec table)         */
/* ==================================================================== */

typedef struct {
    const char *suffix;
    int code;
} CondEntry;

static const CondEntry COND_TABLE[] = {
    { "EQ", 0 }, { "NE", 1 }, { "CS", 2 }, { "CC", 3 },
    { "MI", 4 }, { "PL", 5 }, { "VS", 6 }, { "VC", 7 },
    { "HI", 8 }, { "LS", 9 }, { "GE", 10 }, { "LT", 11 },
    { "GT", 12 }, { "LE", 13 }, { "AL", 14 }
};
#define NUM_CONDS (int)(sizeof(COND_TABLE) / sizeof(COND_TABLE[0]))

static int cond_code_for_suffix(const char *suffix) {
    int i;
    for (i = 0; i < NUM_CONDS; i++) {
        if (strcmp(COND_TABLE[i].suffix, suffix) == 0) {
            return COND_TABLE[i].code;
        }
    }
    return -1;
}

/* ==================================================================== */
/* Opcode table                                                         */
/*                                                                       */
/* NOTE: the handout's table lists Memory-read-constant as 0x0C, which  */
/* collides with Divide-constant (also 0x0C). Following the clean       */
/* sequence 0x09 add, 0x0A sub, 0x0B mul, 0x0C div, [gap], 0x0E write,  */
/* 0x0F move -- Memory-read-constant is implemented here as 0x0D.       */
/* Flag this to your TA/professor before submitting if it matters.      */
/* ==================================================================== */

#define OP_ADD_VAR        0x01
#define OP_SUB_VAR        0x02
#define OP_MUL_VAR        0x03
#define OP_DIV_VAR        0x04
#define OP_MEMREAD_VAR    0x05
#define OP_MEMWRITE_VAR   0x06

#define OP_ADD_CONST      0x09
#define OP_SUB_CONST      0x0A
#define OP_MUL_CONST      0x0B
#define OP_DIV_CONST      0x0C
#define OP_MEMREAD_CONST  0x0D  
#define OP_MEMWRITE_CONST 0x0E
#define OP_MOVE_CONST     0x0F

#define OP_BRANCH_BASE    0x10

/* ==================================================================== */
/* Tokenizer                                                            */
/* ==================================================================== */

static int is_ident_char(char c) { return isalnum((unsigned char)c); }

int tokenize_line(const char *line, Token *tokens, int max_tokens) {
    int n = 0;
    const char *p = line;

    while (*p != '\0' && n < max_tokens) {
        /* skip whitespace */
        if (isspace((unsigned char)*p)) { p++; continue; }

        /* comment: everything after '%' is ignored */
        if (*p == '%') break;

        /* '.' introduces a label -- either a label *definition* (must then
         * be the only token on the line, checked by the parser) or a
         * branch *target*, e.g. "BEQ .exit" (a later token on the line). */
        if (*p == '.') {
            p++;
            char buf[MAX_LABEL_LEN];
            int len = 0;
            while (is_ident_char(*p) && len < MAX_LABEL_LEN - 1) {
                buf[len++] = *p++;
            }
            buf[len] = '\0';
            if (len == 0) return -1;
            tokens[n].type = TOK_LABEL;
            strncpy(tokens[n].text, buf, MAX_LABEL_LEN);
            n++;
            continue;
        }

        if (*p == '=') { tokens[n].type = TOK_ASSIGN; n++; p++; continue; }
        if (*p == '+') { tokens[n].type = TOK_PLUS;   n++; p++; continue; }
        if (*p == '-') { tokens[n].type = TOK_MINUS;  n++; p++; continue; }
        if (*p == '*') { tokens[n].type = TOK_STAR;   n++; p++; continue; }
        if (*p == '/') { tokens[n].type = TOK_SLASH;  n++; p++; continue; }
        if (*p == '[') { tokens[n].type = TOK_LBRACKET; n++; p++; continue; }
        if (*p == ']') { tokens[n].type = TOK_RBRACKET; n++; p++; continue; }
        if (*p == ';') { p++; continue; } /* tolerate stray semicolons */
        if (*p == ',') { p++; continue; } /* tolerate comma-separated args, e.g. "Read x1, 0" */

        if (isdigit((unsigned char)*p)) {
            int val = 0;
            while (isdigit((unsigned char)*p)) {
                val = val * 10 + (*p - '0');
                p++;
            }
            if (val > 255) return -1; /* constants are 0..255 */
            tokens[n].type = TOK_NUMBER;
            tokens[n].value = val;
            n++;
            continue;
        }

        if (isalpha((unsigned char)*p)) {
            char buf[MAX_LABEL_LEN];
            int len = 0;
            while (is_ident_char(*p) && len < MAX_LABEL_LEN - 1) {
                buf[len++] = *p++;
            }
            buf[len] = '\0';

            if (buf[0] == 'x' && isdigit((unsigned char)buf[1])) {
                int regnum = atoi(buf + 1);
                if (regnum < 0 || regnum > 255) return -1;
                tokens[n].type = TOK_IDENT;
                tokens[n].value = regnum;
                n++;
                continue;
            }
            if (strcmp(buf, "Read") == 0) { tokens[n].type = TOK_KW_READ;  n++; continue; }
            if (strcmp(buf, "Write") == 0) { tokens[n].type = TOK_KW_WRITE; n++; continue; }
            if (buf[0] == 'B') {
                int code = cond_code_for_suffix(buf + 1);
                if (code < 0) return -1;
                tokens[n].type = TOK_BRANCH;
                tokens[n].value = code;
                strncpy(tokens[n].text, buf, MAX_LABEL_LEN);
                n++;
                continue;
            }
            return -1; /* unrecognized identifier */
        }

        return -1; /* unrecognized character */
    }

    if (n < max_tokens) tokens[n].type = TOK_EOL;
    return n;
}

/* ==================================================================== */
/* Label table (shared between pass 1 and pass 2)                       */
/* ==================================================================== */

typedef struct {
    char name[MAX_LABEL_LEN];
    int instr_index; /* index into the emitted (non-label) instruction stream */
} LabelEntry;

static LabelEntry g_labels[MAX_LABELS];
static int g_num_labels = 0;

static void label_table_reset(void) { g_num_labels = 0; }

static int label_table_add(const char *name, int instr_index) {
    if (g_num_labels >= MAX_LABELS) return -1;
    strncpy(g_labels[g_num_labels].name, name, MAX_LABEL_LEN);
    g_labels[g_num_labels].instr_index = instr_index;
    g_num_labels++;
    return 0;
}

static int label_table_lookup(const char *name) {
    int i;
    for (i = 0; i < g_num_labels; i++) {
        if (strcmp(g_labels[i].name, name) == 0) return g_labels[i].instr_index;
    }
    return -1;
}

/* ==================================================================== */
/* Parser -- pass 1: just find labels and count real instructions       */
/* ==================================================================== */

static int pass1_find_labels(const char *srcfile) {
    FILE *fp = fopen(srcfile, "r");
    if (!fp) { fprintf(stderr, "compiler: cannot open %s\n", srcfile); return -1; }

    char line[MAX_LINE_LEN];
    int line_no = 0;
    int instr_index = 0; /* counts only real (non-label, non-blank) lines */

    label_table_reset();

    while (fgets(line, sizeof(line), fp)) {
        line_no++;
        Token toks[MAX_TOKENS_PER_LINE];
        int n = tokenize_line(line, toks, MAX_TOKENS_PER_LINE);
        if (n < 0) { fprintf(stderr, "compiler: lex error on line %d\n", line_no); fclose(fp); return -1; }
        if (n == 0) continue; /* blank line or comment-only line */

        if (toks[0].type == TOK_LABEL) {
            if (n != 1) {
                fprintf(stderr, "compiler: line %d: label must be alone on its line\n", line_no);
                fclose(fp);
                return -1;
            }
            if (label_table_add(toks[0].text, instr_index) != 0) {
                fprintf(stderr, "compiler: too many labels\n");
                fclose(fp);
                return -1;
            }
            continue; /* labels don't consume an instruction slot */
        }

        instr_index++;
    }

    fclose(fp);
    return 0;
}

/* ==================================================================== */
/* Parser -- pass 2: build the IR, one entry per real instruction       */
/* ==================================================================== */

/* Parses "IDENT op (IDENT|NUMBER)" arithmetic where op is +,-,*,/ and
 * fills in an IR_ADD/SUB/MUL/DIV instruction. toks[0] is the operand1
 * IDENT, toks[1] is the operator, toks[2] is operand2. dest already
 * known by caller. Returns 0 on success. */
static int parse_arith(Token *toks, int n, IRInstr *ir) {
    if (n != 3) return -1;
    if (toks[0].type != TOK_IDENT) return -1;

    switch (toks[1].type) {
        case TOK_PLUS:  ir->type = IR_ADD; break;
        case TOK_MINUS: ir->type = IR_SUB; break;
        case TOK_STAR:  ir->type = IR_MUL; break;
        case TOK_SLASH: ir->type = IR_DIV; break;
        default: return -1;
    }

    ir->src1 = toks[0].value;

    if (toks[2].type == TOK_IDENT) {
        ir->src2 = toks[2].value;
        ir->src2_is_reg = 1;
    } else if (toks[2].type == TOK_NUMBER) {
        ir->src2 = toks[2].value;
        ir->src2_is_reg = 0;
    } else {
        return -1;
    }
    return 0;
}

static int parse_line_pass2(Token *toks, int n, int line_no, IRInstr *ir) {
    memset(ir, 0, sizeof(*ir));
    ir->line_no = line_no;

    /* Branch: Bxx .label */
    if (toks[0].type == TOK_BRANCH) {
        if (n != 2 || toks[1].type != TOK_LABEL) return -1;
        ir->type = IR_BRANCH;
        ir->branch_cond = toks[0].value;
        strncpy(ir->label, toks[1].text, MAX_LABEL_LEN);
        return 0;
    }

    /* Legacy: Read <variable> <address>  ->  opcode 5, dest=target reg,
     * operand1=constant address, operand2=0 (Lab 1's original layout,
     * no var/const split -- kept separate from the new bracket syntax). */
    if (toks[0].type == TOK_KW_READ) {
        if (n != 3 || toks[1].type != TOK_IDENT || toks[2].type != TOK_NUMBER) return -1;
        ir->type = IR_READ_LEGACY;
        ir->dest = toks[1].value;
        ir->src1 = toks[2].value; /* constant address */
        return 0;
    }

    /* Legacy: Write <variable> <address>  ->  opcode 6, dest=value reg
     * being written, operand1=constant address, operand2=0. */
    if (toks[0].type == TOK_KW_WRITE) {
        if (n != 3 || toks[1].type != TOK_IDENT || toks[2].type != TOK_NUMBER) return -1;
        ir->type = IR_WRITE_LEGACY;
        ir->dest = toks[1].value; /* value register being written */
        ir->src1 = toks[2].value; /* constant address */
        return 0;
    }

    /* Everything else starts "IDENT = ..." or "[ ... ] = IDENT" */

    if (toks[0].type == TOK_LBRACKET) {
        /* [ (IDENT|NUMBER) ] = IDENT   -- memory write, bracket form */
        if (n != 5) return -1;
        if (toks[2].type != TOK_RBRACKET || toks[3].type != TOK_ASSIGN) return -1;
        if (toks[4].type != TOK_IDENT) return -1;

        ir->type = IR_MEMWRITE;
        if (toks[1].type == TOK_IDENT) {
            ir->dest = toks[1].value;
            ir->addr_is_reg = 1;
        } else if (toks[1].type == TOK_NUMBER) {
            ir->dest = toks[1].value;
            ir->addr_is_reg = 0;
        } else {
            return -1;
        }
        ir->src2 = toks[4].value;
        ir->src2_is_reg = 1;
        return 0;
    }

    if (toks[0].type == TOK_IDENT && n >= 2 && toks[1].type == TOK_ASSIGN) {
        int dest = toks[0].value;

        /* IDENT = [ (IDENT|NUMBER) ]   -- memory read */
        if (n == 5 && toks[2].type == TOK_LBRACKET && toks[4].type == TOK_RBRACKET) {
            ir->type = IR_MEMREAD;
            ir->dest = dest;
            if (toks[3].type == TOK_IDENT) {
                ir->src2 = toks[3].value;
                ir->src2_is_reg = 1;
            } else if (toks[3].type == TOK_NUMBER) {
                ir->src2 = toks[3].value;
                ir->src2_is_reg = 0;
            } else {
                return -1;
            }
            return 0;
        }

        /* IDENT = NUMBER   -- data movement */
        if (n == 3 && toks[2].type == TOK_NUMBER) {
            ir->type = IR_MOVE;
            ir->dest = dest;
            ir->src2 = toks[2].value;
            ir->src2_is_reg = 0;
            return 0;
        }

        /* IDENT = IDENT op (IDENT|NUMBER)   -- arithmetic */
        if (n == 5) {
            if (parse_arith(&toks[2], 3, ir) != 0) return -1;
            ir->type = ir->type; /* set by parse_arith */
            ir->dest = dest;
            return 0;
        }
    }

    return -1;
}

int parse_program(const char *srcfile, IRInstr *instrs, int max_instrs) {
    if (pass1_find_labels(srcfile) != 0) return -1;

    FILE *fp = fopen(srcfile, "r");
    if (!fp) { fprintf(stderr, "compiler: cannot open %s\n", srcfile); return -1; }

    char line[MAX_LINE_LEN];
    int line_no = 0;
    int count = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_no++;
        Token toks[MAX_TOKENS_PER_LINE];
        int n = tokenize_line(line, toks, MAX_TOKENS_PER_LINE);
        if (n < 0) { fprintf(stderr, "compiler: lex error on line %d\n", line_no); fclose(fp); return -1; }
        if (n == 0) continue;
        if (toks[0].type == TOK_LABEL) continue; /* handled in pass 1 */

        if (count >= max_instrs) {
            fprintf(stderr, "compiler: too many instructions (max %d)\n", max_instrs);
            fclose(fp);
            return -1;
        }

        if (parse_line_pass2(toks, n, line_no, &instrs[count]) != 0) {
            fprintf(stderr, "compiler: syntax error on line %d\n", line_no);
            fclose(fp);
            return -1;
        }

        if (instrs[count].type == IR_BRANCH) {
            int target = label_table_lookup(instrs[count].label);
            if (target < 0) {
                fprintf(stderr, "compiler: line %d: unknown label '%s'\n", line_no, instrs[count].label);
                fclose(fp);
                return -1;
            }
            instrs[count].offset = target - count; /* instruction-count units */
        }

        count++;
    }

    fclose(fp);
    return count;
}

/* ==================================================================== */
/* Code generation                                                      */
/* ==================================================================== */

static int encode_instruction(IRInstr *ir, unsigned char out[4]) {
    int opcode, dest = 0, op1 = 0, op2 = 0;

    switch (ir->type) {
        case IR_ADD:
            opcode = ir->src2_is_reg ? OP_ADD_VAR : OP_ADD_CONST;
            dest = ir->dest; op1 = ir->src1; op2 = ir->src2;
            break;
        case IR_SUB:
            opcode = ir->src2_is_reg ? OP_SUB_VAR : OP_SUB_CONST;
            dest = ir->dest; op1 = ir->src1; op2 = ir->src2;
            break;
        case IR_MUL:
            opcode = ir->src2_is_reg ? OP_MUL_VAR : OP_MUL_CONST;
            dest = ir->dest; op1 = ir->src1; op2 = ir->src2;
            break;
        case IR_DIV:
            opcode = ir->src2_is_reg ? OP_DIV_VAR : OP_DIV_CONST;
            dest = ir->dest; op1 = ir->src1; op2 = ir->src2;
            break;
        case IR_MOVE:
            opcode = OP_MOVE_CONST; /* language only supports constant data movement */
            dest = ir->dest; op1 = 0; op2 = ir->src2;
            break;
        case IR_MEMREAD:
            opcode = ir->src2_is_reg ? OP_MEMREAD_VAR : OP_MEMREAD_CONST;
            dest = ir->dest; op1 = 0; op2 = ir->src2;
            break;
        case IR_MEMWRITE:
            opcode = ir->addr_is_reg ? OP_MEMWRITE_VAR : OP_MEMWRITE_CONST;
            dest = ir->dest; op1 = 0; op2 = ir->src2;
            break;
        case IR_READ_LEGACY:
            /* Lab 1 layout: opcode 5 always, address in operand1, operand2=0 */
            opcode = OP_MEMREAD_VAR; /* == 0x05, reused as the one-and-only Read opcode */
            dest = ir->dest; op1 = ir->src1; op2 = 0;
            break;
        case IR_WRITE_LEGACY:
            /* Lab 1 layout: opcode 6 always, address in operand1, operand2=0 */
            opcode = OP_MEMWRITE_VAR; /* == 0x06, reused as the one-and-only Write opcode */
            dest = ir->dest; op1 = ir->src1; op2 = 0;
            break;
        case IR_BRANCH: {
            opcode = OP_BRANCH_BASE + ir->branch_cond;
            dest = 0; op1 = 0;
            op2 = ir->offset & 0xFF; /* two's complement byte */
            break;
        }
        default:
            return -1;
    }

    if (dest < 0 || dest > 255 || op1 < 0 || op1 > 255) return -1;

    out[0] = (unsigned char)opcode;
    out[1] = (unsigned char)dest;
    out[2] = (unsigned char)op1;
    out[3] = (unsigned char)(op2 & 0xFF);
    return 0;
}

int emit_bytecode(IRInstr *instrs, int count, const char *outfile) {
    FILE *fp = fopen(outfile, "w");
    if (!fp) { fprintf(stderr, "compiler: cannot open %s for writing\n", outfile); return -1; }

    int i;
    for (i = 0; i < count; i++) {
        unsigned char b[4];
        if (encode_instruction(&instrs[i], b) != 0) {
            fprintf(stderr, "compiler: cannot encode instruction on line %d\n", instrs[i].line_no);
            fclose(fp);
            return -1;
        }
        fprintf(fp, "%X %X %X %X\n", b[0], b[1], b[2], b[3]);
    }

    fclose(fp);
    return 0;
}

int compile(const char *srcfile, const char *bytefile) {
    IRInstr instrs[MAX_INSTRUCTIONS];
    int count = parse_program(srcfile, instrs, MAX_INSTRUCTIONS);
    if (count < 0) return -1;
    return emit_bytecode(instrs, count, bytefile);
}
