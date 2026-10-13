#ifndef COMPILER_H
#define COMPILER_H

#define MAX_LINE_LEN        256
#define MAX_TOKENS_PER_LINE  16
#define MAX_INSTRUCTIONS     64   /* instruction memory is 256 bytes / 4 bytes each */
#define MAX_LABEL_LEN        32
#define MAX_LABELS           64

/* ------------------------------------------------------------------ */
/* Tokenizer                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    TOK_IDENT,      /* x0 .. x255                       */
    TOK_NUMBER,     /* 0 .. 255                          */
    TOK_ASSIGN,     /* =                                 */
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_LBRACKET,   /* [                                 */
    TOK_RBRACKET,   /* ]                                 */
    TOK_LABEL,      /* .name                             */
    TOK_BRANCH,     /* BEQ, BNE, BAL, ...                */
    TOK_KW_READ,    /* legacy: Read                      */
    TOK_KW_WRITE,   /* legacy: Write                     */
    TOK_EOL
} TokenType;

typedef struct {
    TokenType type;
    int  value;               /* register number / constant / branch cond code */
    char text[MAX_LABEL_LEN]; /* raw text: label name or branch mnemonic       */
} Token;

/* Tokenizes one source line (comment already assumed NOT stripped -
 * tokenize_line strips everything after '%' itself).
 * Returns number of tokens written (a trailing TOK_EOL is NOT counted,
 * but IS written if there is room). Returns -1 on a lexical error. */
int tokenize_line(const char *line, Token *tokens, int max_tokens);

/* ------------------------------------------------------------------ */
/* Intermediate representation                                        */
/* ------------------------------------------------------------------ */

typedef enum {
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_MOVE,
    IR_MEMREAD,        /* new Lab-2 bracket syntax: x = [reg] or x = [const] */
    IR_MEMWRITE,       /* new Lab-2 bracket syntax: [reg] = x or [const] = x */
    IR_READ_LEGACY,    /* Lab-1 style: Read <variable> <address>            */
    IR_WRITE_LEGACY,   /* Lab-1 style: Write <variable> <address>           */
    IR_BRANCH
} IRType;

typedef struct {
    IRType type;

    int dest;          /* ADD/SUB/MUL/DIV/MOVE/MEMREAD: destination register.
                           MEMWRITE: register holding the address, OR the
                           literal address itself -- see addr_is_reg.       */
    int addr_is_reg;   /* MEMWRITE only: 1 -> dest is a register number to
                           dereference for the address, 0 -> dest IS the
                           literal address.                                 */

    int src1;           /* operand1 register number (0 when unused)        */

    int src2;            /* operand2: register number, OR a literal
                            constant, OR (MEMWRITE) the register holding
                            the value to write -- see src2_is_reg.          */
    int src2_is_reg;     /* 1 -> src2 is a register number, 0 -> literal    */

    int branch_cond;     /* 0..14, valid when type == IR_BRANCH             */
    char label[MAX_LABEL_LEN]; /* branch target name, pre-resolution        */
    int offset;           /* resolved (target_index - this_index), filled
                            in during pass 2                                */

    int line_no;          /* source line, for error messages                */
} IRInstr;

/* Parses srcfile into instrs[]. Returns instruction count, or -1 on error. */
int parse_program(const char *srcfile, IRInstr *instrs, int max_instrs);

/* Writes bytecode for instrs[0..count) to outfile ("program.byte" format:
 * one instruction per line, four space separated hex byte values). */
int emit_bytecode(IRInstr *instrs, int count, const char *outfile);

/* High level driver: parse_program() + emit_bytecode(). Returns 0 on
 * success, -1 on failure. */
int compile(const char *srcfile, const char *bytefile);

#endif /* COMPILER_H */
