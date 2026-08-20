#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "compiler.h"
#include "opcodes.h"

#define MAX_LINES   512
#define MAX_INSTR   64   /* 256 bytes of instruction memory / 4 bytes each */
#define MAX_LABELS  128
#define LINE_LEN    256

typedef struct {
    int opcode, dest, src1, src2;
    /* if this instruction is an unresolved branch, branch_label is set
     * (non-empty) and gets patched into src2 (as a signed offset) once
     * all labels are known */
    char branch_label[64];
    int  is_branch;
} Instr;

typedef struct {
    char name[64];
    int  index; /* instruction index the label points at */
} Label;

static Instr    instrs[MAX_INSTR];
static int      instr_count = 0;
static Label    labels[MAX_LABELS];
static int      label_count = 0;

/* ---------- small string helpers ---------------------------------- */

static void trim(char *s) {
    /* strip trailing \r\n and whitespace */
    int len = (int)strlen(s);
    while (len > 0 && (isspace((unsigned char)s[len-1]))) s[--len] = '\0';
    /* strip leading whitespace */
    int start = 0;
    while (s[start] && isspace((unsigned char)s[start])) start++;
    if (start > 0) memmove(s, s + start, strlen(s + start) + 1);
}

static void strip_comment(char *s) {
    char *p = strchr(s, '%');
    if (p) *p = '\0';
}

static int is_all_digits(const char *s) {
    if (!*s) return 0;
    for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) return 0;
    return 1;
}

/* Token classification for an operand: 'x' -> integer register,
 * 'v' -> vector register, 'c' -> constant, '?' -> invalid */
static char classify_operand(const char *tok, int *value) {
    if ((tok[0] == 'x' || tok[0] == 'X') && is_all_digits(tok + 1)) {
        *value = atoi(tok + 1);
        return 'x';
    }
    if ((tok[0] == 'v' || tok[0] == 'V') && is_all_digits(tok + 1)) {
        *value = atoi(tok + 1);
        return 'v';
    }
    if (is_all_digits(tok)) {
        long v = atol(tok);
        if (v > 255) {
            fprintf(stderr,
                "Error: constant %ld out of range - constants must be 0-255 "
                "(build larger addresses/values at runtime with arithmetic "
                "instead of a single immediate)\n", v);
            exit(1);
        }
        *value = (int)v;
        return 'c';
    }
    return '?';
}

static Instr *emit(void) {
    if (instr_count >= MAX_INSTR) {
        fprintf(stderr, "Error: program too large (max %d instructions / %d bytes of instruction memory)\n",
                MAX_INSTR, MAX_INSTR * 4);
        exit(1);
    }
    Instr *ins = &instrs[instr_count++];
    memset(ins, 0, sizeof(*ins));
    return ins;
}

/* ---------- pass 1: strip/clean source into an array of lines ------ */

static char clean_lines[MAX_LINES][LINE_LEN];
static int  clean_line_count = 0;

static void load_and_clean(const char *srcfile) {
    FILE *fp = fopen(srcfile, "r");
    if (!fp) {
        fprintf(stderr, "Error: could not open source file '%s'\n", srcfile);
        exit(1);
    }
    char raw[LINE_LEN];
    while (fgets(raw, sizeof(raw), fp)) {
        strip_comment(raw);
        trim(raw);
        if (raw[0] == '\0') continue;
        if (clean_line_count >= MAX_LINES) {
            fprintf(stderr, "Error: source file too long\n");
            exit(1);
        }
        strncpy(clean_lines[clean_line_count], raw, LINE_LEN - 1);
        clean_line_count++;
    }
    fclose(fp);
}

/* Detects a source file that is already compiled bytecode: every clean
 * line is exactly 4 whitespace-separated hex tokens. */
static int looks_like_bytecode(void) {
    if (clean_line_count == 0) return 0;
    for (int i = 0; i < clean_line_count; i++) {
        unsigned int a, b, c, d;
        char extra[8];
        int n = sscanf(clean_lines[i], "%x %x %x %x %7s", &a, &b, &c, &d, extra);
        if (n != 4) return 0;
        if (a > 255 || b > 255 || c > 255 || d > 255) return 0;
    }
    return 1;
}

/* ---------- label pass --------------------------------------------- */

static void add_label(const char *name, int index) {
    if (label_count >= MAX_LABELS) {
        fprintf(stderr, "Error: too many labels\n");
        exit(1);
    }
    strncpy(labels[label_count].name, name, sizeof(labels[0].name) - 1);
    labels[label_count].index = index;
    label_count++;
}

static int find_label(const char *name) {
    for (int i = 0; i < label_count; i++)
        if (strcmp(labels[i].name, name) == 0) return labels[i].index;
    fprintf(stderr, "Error: undefined label '.%s'\n", name);
    exit(1);
}

/* ---------- branch line parsing ------------------------------------ */

static const struct { const char *suffix; int code; } BRANCH_TABLE[] = {
    {"EQ", BC_EQ}, {"NE", BC_NE}, {"CS", BC_CS}, {"CC", BC_CC},
    {"MI", BC_MI}, {"PL", BC_PL}, {"VS", BC_VS}, {"VC", BC_VC},
    {"HI", BC_HI}, {"LS", BC_LS}, {"GE", BC_GE}, {"LT", BC_LT},
    {"GT", BC_GT}, {"LE", BC_LE}, {"AL", BC_AL},
};
#define NUM_BRANCH_CODES (int)(sizeof(BRANCH_TABLE) / sizeof(BRANCH_TABLE[0]))

static int is_branch_line(const char *line) {
    return (line[0] == 'B' && isupper((unsigned char)line[1]) && isupper((unsigned char)line[2]));
}

static void parse_branch(const char *line) {
    char suffix[8] = {0};
    char target[64] = {0};
    strncpy(suffix, line + 1, 2);
    /* find the label after the suffix: skip suffix + whitespace, expect '.' */
    const char *p = line + 3;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '.') {
        fprintf(stderr, "Error: malformed branch instruction '%s' (expected .label)\n", line);
        exit(1);
    }
    p++; /* skip '.' */
    strncpy(target, p, sizeof(target) - 1);
    trim(target);

    int code = -1;
    for (int i = 0; i < NUM_BRANCH_CODES; i++) {
        if (strcmp(suffix, BRANCH_TABLE[i].suffix) == 0) { code = BRANCH_TABLE[i].code; break; }
    }
    if (code < 0) {
        fprintf(stderr, "Error: unknown branch suffix '%s' in '%s'\n", suffix, line);
        exit(1);
    }

    Instr *ins = emit();
    ins->opcode = OP_BRANCH_BASE + code;
    ins->dest = 0;
    ins->src1 = 0;
    ins->is_branch = 1;
    strncpy(ins->branch_label, target, sizeof(ins->branch_label) - 1);
}

/* ---------- legacy Read/Write --------------------------------------- */

static int is_legacy_read(const char *line)  { return strncmp(line, "Read ",  5) == 0; }
static int is_legacy_write(const char *line) { return strncmp(line, "Write ", 6) == 0; }

static void parse_legacy_read(const char *line) {
    char varTok[64], addrTok[64];
    if (sscanf(line + 5, "%63s %63s", varTok, addrTok) != 2) {
        fprintf(stderr, "Error: malformed Read instruction '%s'\n", line);
        exit(1);
    }
    int v1, v2;
    char t1 = classify_operand(varTok, &v1);
    char t2 = classify_operand(addrTok, &v2);
    if (t1 != 'x' || t2 != 'c') {
        fprintf(stderr, "Error: 'Read <xreg> <constant address>' expected, got '%s'\n", line);
        exit(1);
    }
    Instr *ins = emit();
    ins->opcode = OP_MEMREAD_CONST;
    ins->dest = v1; ins->src1 = 0; ins->src2 = v2;
}

static void parse_legacy_write(const char *line) {
    char varTok[64], addrTok[64];
    if (sscanf(line + 6, "%63s %63s", varTok, addrTok) != 2) {
        fprintf(stderr, "Error: malformed Write instruction '%s'\n", line);
        exit(1);
    }
    int v1, v2;
    char t1 = classify_operand(varTok, &v1);
    char t2 = classify_operand(addrTok, &v2);
    if (t1 != 'x' || t2 != 'c') {
        fprintf(stderr, "Error: 'Write <xreg> <constant address>' expected, got '%s'\n", line);
        exit(1);
    }
    Instr *ins = emit();
    ins->opcode = OP_MEMWRITE_CONST;
    ins->dest = v2; ins->src1 = 0; ins->src2 = v1;
}

/* ---------- '=' style statements ------------------------------------ */

static void split_at_equals(char *line, char *lhs, char *rhs) {
    char *eq = strchr(line, '=');
    if (!eq) {
        fprintf(stderr, "Error: no '=' found in statement '%s'\n", line);
        exit(1);
    }
    *eq = '\0';
    strcpy(lhs, line);
    strcpy(rhs, eq + 1);
    trim(lhs);
    trim(rhs);
}

/* extracts the token inside [ ... ], trimmed */
static void extract_bracket(const char *s, char *out) {
    const char *open = strchr(s, '[');
    const char *close = strchr(s, ']');
    if (!open || !close || close < open) {
        fprintf(stderr, "Error: malformed bracket expression '%s'\n", s);
        exit(1);
    }
    int len = (int)(close - open - 1);
    strncpy(out, open + 1, len);
    out[len] = '\0';
    trim(out);
}

static void parse_memory_write(const char *lhs, const char *rhs) {
    char addrBuf[64];
    extract_bracket(lhs, addrBuf);
    int addrVal, valVal;
    char addrType = classify_operand(addrBuf, &addrVal);
    char valType  = classify_operand(rhs, &valVal);

    Instr *ins = emit();
    if (valType == 'v') { /* vector store */
        ins->src2 = valVal;
        if (addrType == 'x') { ins->opcode = OP_VMEMWRITE_VAR;   ins->dest = addrVal; }
        else if (addrType == 'c') { ins->opcode = OP_VMEMWRITE_CONST; ins->dest = addrVal; }
        else { fprintf(stderr, "Error: bad address in '[%s] = %s'\n", addrBuf, rhs); exit(1); }
    } else if (valType == 'x') { /* scalar store */
        ins->src2 = valVal;
        if (addrType == 'x') { ins->opcode = OP_MEMWRITE_VAR;   ins->dest = addrVal; }
        else if (addrType == 'c') { ins->opcode = OP_MEMWRITE_CONST; ins->dest = addrVal; }
        else { fprintf(stderr, "Error: bad address in '[%s] = %s'\n", addrBuf, rhs); exit(1); }
    } else {
        fprintf(stderr, "Error: right-hand side of a memory write must be a register, got '%s'\n", rhs);
        exit(1);
    }
    ins->src1 = 0;
}

static void parse_memory_read(const char *lhs, const char *rhs) {
    char addrBuf[64];
    extract_bracket(rhs, addrBuf);
    int destVal, addrVal;
    char destType = classify_operand(lhs, &destVal);
    char addrType = classify_operand(addrBuf, &addrVal);

    Instr *ins = emit();
    ins->src1 = 0;
    if (destType == 'v') {
        ins->dest = destVal;
        if (addrType == 'x') { ins->opcode = OP_VMEMREAD_VAR;   ins->src2 = addrVal; }
        else if (addrType == 'c') { ins->opcode = OP_VMEMREAD_CONST; ins->src2 = addrVal; }
        else { fprintf(stderr, "Error: bad address in '%s = [%s]'\n", lhs, addrBuf); exit(1); }
    } else if (destType == 'x') {
        ins->dest = destVal;
        if (addrType == 'x') { ins->opcode = OP_MEMREAD_VAR;   ins->src2 = addrVal; }
        else if (addrType == 'c') { ins->opcode = OP_MEMREAD_CONST; ins->src2 = addrVal; }
        else { fprintf(stderr, "Error: bad address in '%s = [%s]'\n", lhs, addrBuf); exit(1); }
    } else {
        fprintf(stderr, "Error: left-hand side of a memory read must be a register, got '%s'\n", lhs);
        exit(1);
    }
}

static void parse_data_move_or_copy(const char *lhs, const char *rhs) {
    int destVal, rhsVal;
    char destType = classify_operand(lhs, &destVal);
    char rhsType  = classify_operand(rhs, &rhsVal);
    if (destType != 'x' && destType != 'v') {
        fprintf(stderr, "Error: invalid destination '%s'\n", lhs);
        exit(1);
    }
    Instr *ins = emit();
    ins->dest = destVal;
    if (rhsType == 'c') {
        if (destType != 'x') {
            fprintf(stderr, "Error: data movement of a constant is only defined for x-registers ('%s = %s')\n", lhs, rhs);
            exit(1);
        }
        ins->opcode = OP_DATAMOVE_CONST;
        ins->src1 = 0;
        ins->src2 = rhsVal;
    } else if ((destType == 'x' && rhsType == 'x') || (destType == 'v' && rhsType == 'v')) {
        /* plain register copy, compiled as "+ 0" (see compiler.h note) */
        ins->opcode = (destType == 'x') ? OP_ADD_CONST : OP_VADD_CONST;
        ins->src1 = rhsVal;
        ins->src2 = 0;
    } else {
        fprintf(stderr, "Error: cannot assign %s to %s ('%s = %s')\n",
                rhsType == 'v' ? "a vector register" : "an integer register",
                destType == 'v' ? "a vector register" : "an integer register", lhs, rhs);
        exit(1);
    }
}

static void parse_arithmetic(const char *lhs, char *rhs) {
    char op1Tok[64], opChar[4], op2Tok[64];
    if (sscanf(rhs, "%63s %3s %63s", op1Tok, opChar, op2Tok) != 3) {
        fprintf(stderr, "Error: malformed arithmetic expression '%s = %s'\n", lhs, rhs);
        exit(1);
    }
    int destVal, op1Val, op2Val;
    char destType = classify_operand(lhs, &destVal);
    char op1Type  = classify_operand(op1Tok, &op1Val);
    char op2Type  = classify_operand(op2Tok, &op2Val);
    char op = opChar[0];

    Instr *ins = emit();
    ins->dest = destVal;

    if (destType == 'x') {
        if (op1Type != 'x') {
            fprintf(stderr, "Error: first operand of an integer expression must be an x-register ('%s = %s')\n", lhs, rhs);
            exit(1);
        }
        ins->src1 = op1Val;
        int isConst = (op2Type == 'c');
        if (!isConst && op2Type != 'x') {
            fprintf(stderr, "Error: bad second operand '%s' in '%s = %s'\n", op2Tok, lhs, rhs);
            exit(1);
        }
        ins->src2 = op2Val;
        switch (op) {
            case '+': ins->opcode = isConst ? OP_ADD_CONST : OP_ADD_VAR; break;
            case '-': ins->opcode = isConst ? OP_SUB_CONST : OP_SUB_VAR; break;
            case '*': ins->opcode = isConst ? OP_MUL_CONST : OP_MUL_VAR; break;
            case '/': ins->opcode = isConst ? OP_DIV_CONST : OP_DIV_VAR; break;
            default:
                fprintf(stderr, "Error: unsupported integer operator '%c' in '%s = %s'\n", op, lhs, rhs);
                exit(1);
        }
    } else if (destType == 'v') {
        if (op1Type != 'v') {
            fprintf(stderr, "Error: first operand of a vector expression must be a vector register ('%s = %s')\n", lhs, rhs);
            exit(1);
        }
        ins->src1 = op1Val;
        ins->src2 = op2Val;
        if (op != '+' && op != '-' && op != '*') {
            fprintf(stderr, "Error: unsupported vector operator '%c' (only + - * are defined) in '%s = %s'\n", op, lhs, rhs);
            exit(1);
        }
        if (op2Type == 'v') {
            ins->opcode = (op == '+') ? OP_VADD_VEC : (op == '-') ? OP_VSUB_VEC : OP_VMUL_VEC;
        } else if (op2Type == 'x') {
            ins->opcode = (op == '+') ? OP_VADD_SCALARREG : (op == '-') ? OP_VSUB_SCALARREG : OP_VMUL_SCALARREG;
        } else if (op2Type == 'c') {
            ins->opcode = (op == '+') ? OP_VADD_CONST : (op == '-') ? OP_VSUB_CONST : OP_VMUL_CONST;
        } else {
            fprintf(stderr, "Error: bad second operand '%s' in '%s = %s'\n", op2Tok, lhs, rhs);
            exit(1);
        }
    } else {
        fprintf(stderr, "Error: invalid destination '%s'\n", lhs);
        exit(1);
    }
}

/* ---------- top level line dispatch ---------------------------------- */

static void parse_line(char *line) {
    if (line[0] == '.') {
        /* handled entirely in the label pre-pass; nothing to emit */
        return;
    }
    if (is_branch_line(line)) { parse_branch(line); return; }
    if (is_legacy_read(line))  { parse_legacy_read(line);  return; }
    if (is_legacy_write(line)) { parse_legacy_write(line); return; }

    char lhs[LINE_LEN], rhs[LINE_LEN];
    split_at_equals(line, lhs, rhs);

    if (lhs[0] == '[') { parse_memory_write(lhs, rhs); return; }
    if (rhs[0] == '[') { parse_memory_read(lhs, rhs);  return; }

    /* count whitespace-separated tokens in rhs to tell a plain move
     * from a 3-token arithmetic expression */
    char rhsCopy[LINE_LEN];
    strncpy(rhsCopy, rhs, LINE_LEN - 1);
    rhsCopy[LINE_LEN - 1] = '\0';
    int tokCount = 0;
    {
        char *tok = strtok(rhsCopy, " \t");
        while (tok) { tokCount++; tok = strtok(NULL, " \t"); }
    }

    if (tokCount == 1) parse_data_move_or_copy(lhs, rhs);
    else                parse_arithmetic(lhs, rhs);
}

/* ---------- driver ----------------------------------------------------- */

void compile(const char *srcfile, const char *outfile) {
    clean_line_count = 0;
    instr_count = 0;
    label_count = 0;

    load_and_clean(srcfile);

    if (looks_like_bytecode()) {
        /* pass-through: srcfile is already a compiled program.byte */
        FILE *in = fopen(srcfile, "r");
        FILE *out = fopen(outfile, "w");
        if (!in || !out) {
            fprintf(stderr, "Error: could not copy bytecode file through\n");
            exit(1);
        }
        char buf[LINE_LEN];
        while (fgets(buf, sizeof(buf), in)) fputs(buf, out);
        fclose(in); fclose(out);
        return;
    }

    /* label pre-pass: walk the cleaned lines, tracking the instruction
     * index that will be assigned to each non-label line */
    int idx = 0;
    for (int i = 0; i < clean_line_count; i++) {
        if (clean_lines[i][0] == '.') {
            add_label(clean_lines[i] + 1, idx);
        } else {
            idx++;
        }
    }

    for (int i = 0; i < clean_line_count; i++) {
        parse_line(clean_lines[i]);
    }

    /* resolve branch targets now that every label is known */
    for (int i = 0; i < instr_count; i++) {
        if (instrs[i].is_branch) {
            int target = find_label(instrs[i].branch_label);
            int offset = target - i; /* instruction-count delta, see processor.c */
            instrs[i].src2 = offset & 0xFF; /* two's complement byte */
        }
    }

    /* make sure the program actually halts */
    if (instr_count == 0 || instrs[instr_count - 1].opcode != OP_HALT) {
        Instr *ins = emit();
        ins->opcode = OP_HALT;
    }

    FILE *out = fopen(outfile, "w");
    if (!out) {
        fprintf(stderr, "Error: could not write output file '%s'\n", outfile);
        exit(1);
    }
    for (int i = 0; i < instr_count; i++) {
        fprintf(out, "%X %X %X %X\n", instrs[i].opcode, instrs[i].dest, instrs[i].src1, instrs[i].src2);
    }
    fclose(out);
}
