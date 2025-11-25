// vm.h (фрагмент)
#include <stdint.h>
#include <stdlib.h>

#define MEM_SIZE (1<<20) // 1 MB for prototyping
#define OPC(i) ((i)>>25)
typedef struct VM {
    uint32_t regs[32];
    uint32_t ip;
    uint32_t sp;
    uint32_t flags;
    uint8_t *mem;
    int running;
    void (*opcode_handlers[256])(struct VM*, uint32_t instr);
} VM;

VM* vm_create(size_t mem_size);
void vm_destroy(VM* vm);
void vm_run(VM* vm);

// helper to read 32-bit instruction (little-endian)
static inline uint32_t vm_fetch32(VM* vm) {
    uint32_t addr = vm->ip;
    uint32_t v = *(uint32_t*)&vm->mem[addr];
    return v;
}
