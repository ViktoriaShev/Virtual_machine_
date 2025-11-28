// Простой ассемблер для VM: текст -> binary (32-bit words)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#define MAX_LINE 512

typedef struct { const char *name; int code; } OpEntry;

static OpEntry op_table[] = {
    {"add",0},{"sub",1},{"mul",2},{"div",3},{"mod",4},{"expt",5},{"abs",6},{"sqrt",7},
    {"ln",8},{"log",9},{"exp",10},{"sin",11},{"cos",12},{"tan",13},{"asin",14},{"acos",15},{"atan",16},
    {"and",17},{"or",18},{"xor",19},{"not",20},
    {"eq",21},{"ne",22},{"gt",23},{"ge",24},{"lt",25},{"le",26},
    {"time",27},{"date",28},{"tod",29},{"dt",30},{"add_time",31},{"sub_time",32},
    {"year",33},{"month",34},{"day",35},{"hour",36},{"minute",37},{"second",38},
    {"len",39},{"concat",40},{"left",41},{"right",42},{"mid",43},{"insert",44},{"delete",45},{"replace",46},
    {"ton",47},{"tof",48},{"tp",49},
    {"ctu",50},{"ctd",51},{"ctud",52},
    {"limit",53},{"sel",54},{"mux",55},
    {"jmp",56},{"jmp_if",57},{"jmp_if_not",58},
    {"halt",59},  
    {NULL,-1}
};


static int opcode_from_name(const char* name) {
    for (OpEntry *e = op_table; e->name; e++) {
        if (strcasecmp(e->name, name)==0) return e->code;
    }
    return -1;
}

// parse operand: accepts "R12" or "12" or "0xFF"
static int parse_operand(const char *tok, int *out) {
    if (!tok || !*tok) return -1;
    // skip leading spaces
    while (*tok && isspace((unsigned char)*tok)) tok++;
    if (tolower((unsigned char)tok[0])=='r') {
        // register
        char *end;
        long v = strtol(tok+1, &end, 0);
        if (end == tok+1) return -1;
        *out = (int)v;
        return 0;
    } else {
        char *end;
        long v = strtol(tok, &end, 0);
        if (end == tok) return -1;
        *out = (int)v;
        return 0;
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.asm> <output.bin>\n", argv[0]);
        return 1;
    }
    FILE *in = fopen(argv[1], "r");
    if (!in) { perror("fopen input"); return 1; }
    FILE *out = fopen(argv[2], "wb");
    if (!out) { perror("fopen output"); fclose(in); return 1; }

    char line[MAX_LINE];
    unsigned long lineno = 0;
    while (fgets(line, sizeof(line), in)) {
        lineno++;
        // trim leading spaces
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        // skip empty lines and comments
        if (*p == '\0' || *p == '\n' || *p == ';' || *p == '#') continue;

        // remove trailing newline
        char *nl = strchr(p, '\n'); if (nl) *nl = '\0';
        // remove inline comments starting with ';' or '#'
        char *cpos = strpbrk(p, ";#");
        if (cpos) *cpos = '\0';

        // tokenize
        char *tok = strtok(p, " \t,");
        if (!tok) continue;
        int op = opcode_from_name(tok);
        if (op < 0) {
            fprintf(stderr, "Line %lu: unknown opcode '%s'\n", lineno, tok);
            fclose(in); fclose(out); return 1;
        }

        // default operands
        int A = 0, B = 0, C = 0;
        char *tA = strtok(NULL, " \t,");
        char *tB = strtok(NULL, " \t,");
        char *tC = strtok(NULL, " \t,");
        if (tA) {
            if (parse_operand(tA, &A) != 0) { fprintf(stderr,"Line %lu: bad operand A '%s'\n",lineno,tA); fclose(in); fclose(out); return 1; }
        }
        if (tB) {
            if (parse_operand(tB, &B) != 0) { fprintf(stderr,"Line %lu: bad operand B '%s'\n",lineno,tB); fclose(in); fclose(out); return 1; }
        }
        if (tC) {
            if (parse_operand(tC, &C) != 0) { fprintf(stderr,"Line %lu: bad operand C '%s'\n",lineno,tC); fclose(in); fclose(out); return 1; }
        }

        // range checks
        if (op < 0 || op > 0x7F) { fprintf(stderr,"Line %lu: opcode out of range\n",lineno); fclose(in); fclose(out); return 1; }
        if (A < 0 || A > 0xFF) { fprintf(stderr,"Line %lu: A out of range (0..255): %d\n",lineno,A); fclose(in); fclose(out); return 1; }
        if (B < 0 || B > 0xFF) { fprintf(stderr,"Line %lu: B out of range (0..255): %d\n",lineno,B); fclose(in); fclose(out); return 1; }
        if (C < 0 || C > 0x1FF) { fprintf(stderr,"Line %lu: C out of range (0..511): %d\n",lineno,C); fclose(in); fclose(out); return 1; }

        uint32_t word = ((uint32_t)op << 25) | ((uint32_t)A << 17) | ((uint32_t)B << 9) | ((uint32_t)C);

        // Для переносимости явно записываем 4 байта в little-endian порядок:
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
    printf("Assembled %s -> %s\n", argv[1], argv[2]);
    return 0;
}
