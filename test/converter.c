// converter.c
// Ассемблер -> бинарник для VM32 (поддержка C immediate '#')
// Usage: asm2bin input.asm output.bin [--raw-halt]

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>

#define MAX_LINE 512

typedef enum {
    CFLAG_NONE = 0,
    CFLAG_ALLOW_IMM = 1<<0,   // C может быть immediate (#n)
    CFLAG_C_MUST_REG = 1<<1   // C обязателен регистром (адрес/индекс и т.п.)
} CFlags;

typedef struct {
    const char *name;
    int code;   // оставлено для ясности — используем индекс в op_table как opcode
    CFlags cflags;
} OpEntry;

/* ----- Таблица opcode'ов и политика для поля C -----
   Порядок здесь должен совпадать с порядком в default_op_ex в vm32.c
*/
static OpEntry op_table[] = {
    {"add",0, CFLAG_ALLOW_IMM},
    {"sub",1, CFLAG_ALLOW_IMM},
    {"mul",2, CFLAG_ALLOW_IMM},
    {"div",3, CFLAG_ALLOW_IMM},
    {"mod",4, CFLAG_ALLOW_IMM},
    {"expt",5, CFLAG_ALLOW_IMM},
    {"abs",6, CFLAG_NONE},
    {"sqrt",7, CFLAG_NONE},
    {"ln",8, CFLAG_NONE},
    {"log",9, CFLAG_NONE},
    {"exp",10, CFLAG_NONE},
    {"sin",11, CFLAG_NONE},
    {"cos",12, CFLAG_NONE},
    {"tan",13, CFLAG_NONE},
    {"asin",14, CFLAG_NONE},
    {"acos",15, CFLAG_NONE},
    {"atan",16, CFLAG_NONE},

    {"and",17, CFLAG_ALLOW_IMM},
    {"or",18,  CFLAG_ALLOW_IMM},
    {"xor",19, CFLAG_ALLOW_IMM},
    {"not",20, CFLAG_NONE},

    {"eq",21,  CFLAG_ALLOW_IMM},
    {"ne",22,  CFLAG_ALLOW_IMM},
    {"gt",23,  CFLAG_ALLOW_IMM},
    {"ge",24,  CFLAG_ALLOW_IMM},
    {"lt",25,  CFLAG_ALLOW_IMM},
    {"le",26,  CFLAG_ALLOW_IMM},

    {"time",27, CFLAG_NONE},
    {"date",28, CFLAG_NONE},
    {"tod",29, CFLAG_NONE},
    {"dt",30, CFLAG_NONE},
    {"add_time",31, CFLAG_ALLOW_IMM},
    {"sub_time",32, CFLAG_ALLOW_IMM},

    {"year",33, CFLAG_NONE},
    {"month",34, CFLAG_NONE},
    {"day",35, CFLAG_NONE},
    {"hour",36, CFLAG_NONE},
    {"minute",37, CFLAG_NONE},
    {"second",38, CFLAG_NONE},

    {"len",39, CFLAG_NONE},
    {"concat",40, CFLAG_C_MUST_REG},    // требует C как регистр-адрес
    {"left",41, CFLAG_ALLOW_IMM},
    {"right",42, CFLAG_ALLOW_IMM},
    {"mid",43, CFLAG_ALLOW_IMM},
    {"insert",44, CFLAG_ALLOW_IMM},
    {"delete",45, CFLAG_ALLOW_IMM},
    {"replace",46, CFLAG_ALLOW_IMM},

    {"ton",47, CFLAG_ALLOW_IMM},
    {"tof",48, CFLAG_ALLOW_IMM},
    {"tp",49, CFLAG_ALLOW_IMM},

    {"ctu",50, CFLAG_ALLOW_IMM},
    {"ctd",51, CFLAG_ALLOW_IMM},
    {"ctud",52, CFLAG_C_MUST_REG},

    {"limit",53, CFLAG_ALLOW_IMM},
    {"sel",54, CFLAG_C_MUST_REG},
    {"mux",55, CFLAG_ALLOW_IMM},

    /* NEW IEC/SCADA ops (added) */
    {"rising_edge",56, CFLAG_ALLOW_IMM},
    {"falling_edge",57, CFLAG_ALLOW_IMM},
    {"edge_both",58, CFLAG_ALLOW_IMM},
    {"rs_latch",59, CFLAG_ALLOW_IMM},
    {"sr_latch",60, CFLAG_ALLOW_IMM},
    {"demux",61, CFLAG_ALLOW_IMM}, /* demux: C may be imm (index) or reg */

    {"jmp",62, CFLAG_ALLOW_IMM},
    {"jmp_if",63, CFLAG_ALLOW_IMM},
    {"jmp_if_not",64, CFLAG_ALLOW_IMM},

    {"exit",65, CFLAG_ALLOW_IMM}, // special: if A==0, immediate used as exit code
    {"halt",66, CFLAG_NONE},
    {"nop",67, CFLAG_NONE},

    {NULL,-1, CFLAG_NONE}
};

static int opcode_from_name(const char* name) {
    for (OpEntry *e = op_table; e->name; ++e) {
        if (e->name == NULL) break;
        if (strcasecmp(e->name, name) == 0) {
            return (int)(e - op_table); /* возвращаем индекс в таблице */
        }
    }
    return -1;
}

/* Operand representation */
typedef enum { OP_REG, OP_IMM } OperandType;
typedef struct { OperandType type; int value; } Operand;

/* Парсер операндов:
   - Регистр: R<num> (обязательно для A и B)
   - Для C: R<num> или #<число>
*/
static int parse_reg(const char *tok, int *out_reg) {
    if (!tok || !*tok) return -1;
    if (tolower((unsigned char)tok[0]) != 'r') return -1;
    char *end;
    errno = 0;
    long v = strtol(tok+1, &end, 0);
    if (end == tok+1) return -1;
    if (errno) return -1;
    *out_reg = (int)v;
    return 0;
}

static int parse_immediate(const char *tok, int *out_val) {
    if (!tok || !*tok) return -1;
    if (tok[0] != '#') return -1;
    char *end;
    errno = 0;
    long v = strtol(tok+1, &end, 0);
    if (end == tok+1) return -1;
    if (errno) return -1;
    *out_val = (int)v;
    return 0;
}

/* Проверка диапазонов */
static int check_reg_range(int r) {
    return (r >= 0 && r <= 0xFF);
}
/* Immediate для 8-bit signed: -128 .. +127 */
static int check_imm8_range(int v) {
    return (v >= -128 && v <= 127);
}

int main(int argc, char** argv) {
    int raw_halt = 0;
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.asm> <output.bin> [--raw-halt]\n", argv[0]);
        return 1;
    }
    if (argc >= 4 && strcmp(argv[3],"--raw-halt")==0)
        raw_halt = 1;

    FILE *in = fopen(argv[1], "r");
    if (!in) { perror("fopen input"); return 1; }
    FILE *out = fopen(argv[2], "wb");
    if (!out) { perror("fopen output"); fclose(in); return 1; }

    /* Выводим краткий список op-кодов, где C может быть immediate */
    fprintf(stderr, "Assembler: C immediate accepted for opcodes: ");
    for (OpEntry *e = op_table; e->name; ++e) {
        if (e->cflags & CFLAG_ALLOW_IMM) fprintf(stderr, "%s ", e->name);
    }
    fprintf(stderr, "\nNote: registers syntax = R<num>, immediate syntax = #<num> (range -128..127 for C)\n");

    char line[MAX_LINE];
    unsigned long lineno = 0;

    while (fgets(line, sizeof(line), in)) {
        lineno++;

        /* trim leading spaces */
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        /* skip empty lines and comments */
        if (*p == '\0' || *p == '\n' || *p == ';') continue;

        /* remove trailing newline */
        char *nl = strchr(p, '\n'); if (nl) *nl = '\0';
        /* remove inline comments (start with ';' only) */
        char *cpos = strchr(p, ';');
        if (cpos) *cpos = '\0';
    
        /* tokenize */
        char *tok = strtok(p, " \t,");
        if (!tok) continue;

        /* auto-map HALT -> EXIT 0 (unless raw-halt) */
        if (!raw_halt && strcasecmp(tok, "halt") == 0) {
            tok = "exit";  /* rewrite (behaviour: EXIT 0) */
        }

        int op_index = opcode_from_name(tok);
        if (op_index < 0) {
            fprintf(stderr, "Line %lu: unknown opcode '%s'\n", lineno, tok);
            fclose(in); fclose(out); return 1;
        }
        OpEntry *opent = &op_table[op_index];

        /* Use table index as opcode (keeps mapping aligned with op_table order) */
        int op = op_index;

        /* default operands text */
        char *tA = strtok(NULL, " \t,");
        char *tB = strtok(NULL, " \t,");
        char *tC = strtok(NULL, " \t,");

        int Areg = 0, Breg = 0;
        Operand Cop; Cop.type = OP_REG;
        Cop.value = 0;

        /* Parse A */
        if (tA) {
            if (parse_reg(tA, &Areg) != 0) {
                fprintf(stderr,"Line %lu: bad operand A '%s' — expected R<n>\n",lineno,tA);
                fclose(in); fclose(out); return 1;
            }
            if (!check_reg_range(Areg)) {
                fprintf(stderr,"Line %lu: A out of range (0..255): %d\n",lineno,Areg);
                fclose(in); fclose(out); return 1;
            }
        }

        /* Parse B */
        if (tB) {
            if (parse_reg(tB, &Breg) != 0) {
                fprintf(stderr,"Line %lu: bad operand B '%s' — expected R<n>\n",lineno,tB);
                fclose(in); fclose(out); return 1;
            }
            if (!check_reg_range(Breg)) {
                fprintf(stderr,"Line %lu: B out of range (0..255): %d\n",lineno,Breg);
                fclose(in); fclose(out); return 1;
            }
        }

        /* Parse C: depends on opcode policy */
        if (tC) {
            int r;
            int immv;
            if (parse_reg(tC, &r) == 0) {
                /* register was provided */
                if (!check_reg_range(r)) {
                    fprintf(stderr,"Line %lu: C register out of range (0..255): %d\n",lineno,r);
                    fclose(in); fclose(out); return 1;
                }
                Cop.type = OP_REG; Cop.value = r;
            } else if (parse_immediate(tC, &immv) == 0) {
                /* immediate provided */
                if (!(opent->cflags & CFLAG_ALLOW_IMM)) {
                    fprintf(stderr,"Line %lu: Opcode '%s' does not allow immediate in C; use a register.\n", lineno, opent->name);
                    fclose(in); fclose(out); return 1;
                }
                if (!check_imm8_range(immv)) {
                    fprintf(stderr,"Line %lu: Immediate out of range for C operand (-128..127): %d\n", lineno, immv);
                    fclose(in); fclose(out); return 1;
                }
                Cop.type = OP_IMM; Cop.value = immv;
            } else {
                fprintf(stderr,"Line %lu: bad operand C '%s' — expected R<n> or #<imm>\n", lineno, tC);
                fclose(in); fclose(out); return 1;
            }
        } else {
            /* No C token -> default C = 0 (register 0) */
            Cop.type = OP_REG; Cop.value = 0;
        }

        /* Check if opcode requires C to be a register */
        if ((opent->cflags & CFLAG_C_MUST_REG) && Cop.type == OP_IMM) {
            fprintf(stderr,"Line %lu: Opcode '%s' requires C to be a register (not immediate).\n", lineno, opent->name);
            fclose(in); fclose(out); return 1;
        }

        /* Range checks for op, A, B done; now build 32-bit word */
        if (op < 0 || op > 0x7F) { fprintf(stderr,"Line %lu: opcode code out of range\n",lineno); fclose(in); fclose(out); return 1; }

        uint32_t Afield = (uint32_t)(Areg & 0xFF);
        uint32_t Bfield = (uint32_t)(Breg & 0xFF);
        uint32_t Cfield = 0;

        if (Cop.type == OP_REG) {
            /* C as register: bit8 == 0, bits0..7 = reg index */
            Cfield = (uint32_t)(Cop.value & 0xFF);
        } else {
            /* C as immediate: set bit8 = 1 (FIMM), bits0..7 = 8-bit two's complement immediate */
            int v = Cop.value;               /* signed -128..127 */
            uint32_t low8 = (uint32_t)((uint8_t)v); /* take low 8 bits */
            Cfield = (1u << 8) | (low8 & 0xFF);     /* FIMM flag in bit8 */
        }
        uint32_t word = ((uint32_t)op << 25) | (Afield << 17) | (Bfield << 9) | Cfield;

        uint8_t bytes[4];
        bytes[0] = (uint8_t)(word & 0xFF);
        bytes[1] = (uint8_t)((word >> 8) & 0xFF);
        bytes[2] = (uint8_t)((word >> 16) & 0xFF);
        bytes[3] = (uint8_t)((word >> 24) & 0xFF);

        if (fwrite(bytes, 1, 4, out) != 4) {
            perror("fwrite");
            fclose(in); fclose(out); return 1;
        }
    }

    fclose(in);
    fclose(out);
    if (raw_halt)
        fprintf(stderr, "Assembled %s -> %s (RAW HALT mode enabled)\n", argv[1], argv[2]);
    else
        fprintf(stderr, "Assembled %s -> %s (HALT mapped to EXIT 0 by default)\n", argv[1], argv[2]);
    return 0;
}
