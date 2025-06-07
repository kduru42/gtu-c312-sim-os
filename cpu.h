#ifndef CPU_H
#define CPU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* sizes… */
#define MEM_SIZE            11000
#define PROGRAM_SIZE        MEM_SIZE
#define USER_MEM_START      1000
#define KERNEL_REG_LIMIT    20
#define filename "log_cpu.txt"


extern FILE *log_cpu;

/* --- instruction representation --- */
typedef enum { 
    OP_SET, OP_CPY, OP_CPYI, OP_CPYI2,
    OP_ADD, OP_ADDI, OP_SUBI,
    OP_JIF, OP_PUSH, OP_POP,
    OP_CALL, OP_RET, OP_HLT, OP_USER,
    OP_SYSCALL,
    OP_BAD
} opcode_t;

typedef struct {
    opcode_t op;
    long     a, b;
} instr_t;

/* --- public CPU struct --- */
typedef enum { KERNEL_MODE, USER_MODE } ukmode_t;

typedef struct Cpu {
    long     memory[MEM_SIZE];
    instr_t  program[PROGRAM_SIZE];
    size_t   program_len;
    ukmode_t   mode;
    bool     halted;
} Cpu;

/* --- API --- */
int   cpu_init       (Cpu *cpu);
int   cpu_load_file  (Cpu *cpu, const char *path);
void  cpu_execute    (Cpu *cpu);
bool  cpu_is_halted  (const Cpu *cpu);

long  cpu_mem_read   (const Cpu *cpu, long addr);
void  cpu_mem_write  (Cpu *cpu, long addr, long val);

#endif /* CPU_H */
