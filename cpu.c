/*
 * cpu.c - GTU‑C312 interpreter (now with SYSCALL handling)
 * ------------------------------------------------------------------
 *  Added instruction:  OP_SYSCALL
 *  - Supports three kernel‑services  (PRN=1, YIELD=2, HLT=3)
 *  - Trap mechanism:
 *        memory[4] ← syscall number
 *        memory[5] ← syscall argument (for PRN)
 *        PC        ← memory[40]        (kernel syscall dispatcher)
 *        mode      ← KERNEL_MODE
 *
 *  All existing functionality (registers, USER mode, trace, etc.) kept.
 */

#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/*=============================================================================
 * Error reporting macro
 *============================================================================*/
static void die_with_location(const char *msg, const char *file, int line) {
    fprintf(stderr, "Error [%s:%d]: %s\n", file, line, msg);
    exit(EXIT_FAILURE);
}
#define DIE(msg) die_with_location(msg, __FILE__, __LINE__)

/*=============================================================================
 * Opcode names (for tracing) – append SYSCALL
 *============================================================================*/
// static const char *opcode_names[] = {
//     "SET", "CPY", "CPYI", "CPYI2", "ADD", "ADDI", "SUBI",
//     "JIF", "PUSH", "POP", "CALL", "RET", "HLT",
//     "USER", "SYSCALL"
// };

static void check_address(const Cpu *cpu, long addr)
{
    if (addr < 0 || addr >= MEM_SIZE)
    DIE("Address out of range");
    
    /* block user threads everywhere *except* the register block 0-19 */
    if (cpu->mode == USER_MODE &&
        addr >= KERNEL_REG_LIMIT && addr < USER_MEM_START)
        {
            DIE("User-mode memory violation");
        }
}
/*=============================================================================
 * Helpers for memory access and address checking
 *============================================================================*/
static long mem_read (const Cpu *cpu, long addr) { check_address(cpu, addr); return cpu->memory[addr]; }
static void mem_write(Cpu *cpu, long addr, long val) { check_address(cpu, addr); cpu->memory[addr] = val; }

/*=============================================================================
 * Maps opcode text → enum value (added "SYSCALL")
 *============================================================================*/
static opcode_t parse_opcode(const char *txt) {
#define EQ(s) if (strcmp(txt, s) == 0)
    EQ("SET")      return OP_SET;
    EQ("CPY")      return OP_CPY;
    EQ("CPYI")     return OP_CPYI;
    EQ("CPYI2")    return OP_CPYI2;
    EQ("ADD")      return OP_ADD;
    EQ("ADDI")     return OP_ADDI;
    EQ("SUBI")     return OP_SUBI;
    EQ("JIF")      return OP_JIF;
    EQ("PUSH")     return OP_PUSH;
    EQ("POP")      return OP_POP;
    EQ("CALL")     return OP_CALL;
    EQ("RET")      return OP_RET;
    EQ("HLT")      return OP_HLT;
    EQ("USER")     return OP_USER;
    EQ("SYSCALL")  return OP_SYSCALL;
#undef EQ
    return OP_BAD;
}

/*=============================================================================
 * How many operands each opcode expects (updated for SYSCALL)
 *============================================================================*/
static int operand_count(opcode_t op) {
    switch (op) {
        case OP_RET:
        case OP_HLT:                   return 0;
        case OP_PUSH:
        case OP_POP:
        case OP_CALL:
        case OP_USER:                  return 1;
        case OP_SYSCALL:               return 2;   /* sub‑code + arg */
        default:                       return 2;   /* everything else */
    }
}

/*=============================================================================
 * Loader – unchanged logic except it now accepts SYSCALL <num> <arg>
 *============================================================================*/
static int load_file(Cpu *cpu, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) { perror(path); return -1; }

    enum { NONE, DATA, CODE } section = NONE;
    char buf[256];

    while (fgets(buf, sizeof buf, fp)) {
        char *hash = strchr(buf, '#'); if (hash) *hash = '\0';
        char *line = buf; while (isspace((unsigned char)*line)) ++line;
        if (!*line) continue;

        if (!strncasecmp(line, "Begin Data Section", 18)){ section = DATA; continue; }
        if (!strncasecmp(line, "End Data Section",   16)){ section = NONE; continue; }
        if (!strncasecmp(line, "Begin Instruction Section", 25)){ section = CODE; continue; }
        if (!strncasecmp(line, "End Instruction Section",   23)){ section = NONE; continue; }

        if (section == DATA) {
            long addr, val;
            if (sscanf(line, "%ld %ld", &addr, &val) != 2) DIE("Invalid data line");
            mem_write(cpu, addr, val);
            continue;
        }

        if (section == CODE) {
            long idx; char opstr[16]; long a=0,b=0;
            if (sscanf(line, "%ld %15s %ld %ld", &idx, opstr, &a, &b) < 2)
                DIE("Invalid instruction line");
            if ((size_t)idx < cpu->program_len) DIE("Duplicate or out-of-order index");
            while ((size_t)idx > cpu->program_len)
                cpu->program[cpu->program_len++] = (instr_t){ OP_SET, 0, 0 };
            opcode_t op = parse_opcode(opstr);
            if (op == OP_BAD) DIE("Unknown opcode");
            else if (op == OP_SYSCALL)
            {
                char sub[16]; long arg = 0;
                int tok = sscanf(line, "%*d %*s %15s %ld", sub, &arg);

                if      (tok >= 1 && strcasecmp(sub, "PRN")   == 0) {
                    if (tok != 2) DIE("SYSCALL PRN needs one operand");
                    a = 2;                 /* sub-code for PRN  */
                    b = arg;               /* address to print  */
                }
                else if (tok >= 1 && strcasecmp(sub, "YIELD") == 0) {
                    if (tok != 1) DIE("SYSCALL YIELD takes no operand");
                    a = 0;  b = 0;         /* sub-code for YIELD */
                }
                else if (tok >= 1 && strcasecmp(sub, "HLT")   == 0) {
                    if (tok != 1) DIE("SYSCALL HLT takes no operand");
                    a = 1;  b = 0;         /* sub-code for HLT   */
                }
                else
                    DIE("Unknown SYSCALL variant");
            }
            else
            {
                int need = operand_count(op);
                /* sscanf already fetched a & b if present; no extra work needed */
                if ((need == 0 && sscanf(line, "%*d %*s")          != 0) ||
                    (need == 1 && sscanf(line, "%*d %*s %ld", &a)  != 1) ||
                    (need == 2 && sscanf(line, "%*d %*s %ld %ld", &a,&b) != 2))
                    DIE("Wrong number of operands");
            }

            cpu->program[cpu->program_len++] = (instr_t){ op, a, b };
        }
    }
    fclose(fp); return 0;
}

/*=============================================================================
 * Public API stubs (unchanged)
 *============================================================================*/
int  cpu_init(Cpu *cpu)                 { if(!cpu) return -1; memset(cpu,0,sizeof*cpu); cpu->mode=KERNEL_MODE; return 0; }
int  cpu_load_file(Cpu *c,const char*p){ return load_file(c,p);} 
bool cpu_is_halted(const Cpu *c)       { return c->halted; }
long cpu_mem_read(const Cpu *c,long a) { return mem_read(c,a);} 
void cpu_mem_write(Cpu *c,long a,long v){ mem_write(c,a,v);} 

/*=============================================================================
* Execute one instruction (SYSCALL added)
*============================================================================*/
void cpu_execute(Cpu *cpu) {
    long pc = mem_read(cpu, 0);
    if (pc < 0 || pc >= (long)cpu->program_len) DIE("PC out of bounds");
    instr_t in = cpu->program[pc];
    
    /* Trace */
    // fprintf(stdout, "[TRACE] PC=%4ld | %-8s %4ld %4ld\n",
    //     pc,
    //     (in.op <= OP_SYSCALL ? opcode_names[in.op] : "???"),
    //     in.a, in.b);

    // instr_used incrementing
    if (pc >= 1000)
    {
        long temp_instr_ptr_cell = mem_read(cpu, 11) * 1000 + 500;
        mem_write(cpu, 17, temp_instr_ptr_cell);
        mem_write(cpu, temp_instr_ptr_cell, mem_read(cpu, temp_instr_ptr_cell) + 1);
    }

        
        
    /* Increment global instruction counter */
    mem_write(cpu, 3, mem_read(cpu, 3) + 1);

    switch (in.op) {
        /* ─── existing cases (unchanged) ─────────────────────────────── */
        case OP_SET:   mem_write(cpu,in.b,in.a);                                   pc++; break;
        case OP_CPY:   mem_write(cpu,in.b,mem_read(cpu,in.a));                     pc++; break;
        case OP_CPYI: { long src = mem_read(cpu,in.a); mem_write(cpu,in.b,mem_read(cpu,src)); pc++; break; }
        case OP_CPYI2: { long src = mem_read(cpu, in.a); long dst = mem_read(cpu, in.b); mem_write(cpu, dst, mem_read(cpu, src)); 
            pc++; break;}
            case OP_ADD:   mem_write(cpu,in.a,mem_read(cpu,in.a)+in.b);                pc++; break;
            case OP_ADDI:  mem_write(cpu,in.a,mem_read(cpu,in.a)+mem_read(cpu,in.b));  pc++; break;
            case OP_SUBI:  mem_write(cpu,in.b,mem_read(cpu,in.a)-mem_read(cpu,in.b));  pc++; break;
            case OP_JIF:   pc = (mem_read(cpu,in.a) <= 0) ? in.b : pc + 1;                       break;
            case OP_PUSH: { long sp = mem_read(cpu,1)-1; mem_write(cpu,1,sp); mem_write(cpu,sp,mem_read(cpu,in.a)); pc++; break; }
            case OP_POP:  { long sp = mem_read(cpu,1); mem_write(cpu,in.a,mem_read(cpu,sp)); mem_write(cpu,1,sp+1); pc++; break; }
            case OP_CALL: { long sp = mem_read(cpu,1)-1; mem_write(cpu,1,sp); mem_write(cpu,sp,pc+1); pc = in.a; break; }
            case OP_RET:  { long sp = mem_read(cpu,1); pc = mem_read(cpu,sp); mem_write(cpu,1,sp+1); break; }
            case OP_HLT:     cpu->halted = true;                                                break;
            case OP_USER: {
                long tgt = mem_read(cpu,in.a);   /* fetch while kernel */
                cpu->mode = USER_MODE;
                pc = tgt;
                break; }

                /* ─── new SYSCALL instruction ────────────────────────────────── */
                case OP_SYSCALL: {
                    cpu->mode = KERNEL_MODE;        /* enter kernel mode   */
                    if (in.a == 2) {                 /* PRN */
                        long addr = in.b;
                        long value = mem_read(cpu, addr);   /* safe read */
                        if (cpu->memory[11] == 1)
                            printf("THREAD 1 (SORT) : %ld\n", value);
                        else if (cpu->memory[11] == 2)
                            printf("THREAD 2 (SEARCH) : %ld\n", value);
                        else
                            printf("THREAD 3 (PRINT) : %ld\n", value);
                                         /* print with newline */
                        
                        fflush(stdout);                     /* ensure immediate output */
                        /* we still want the thread to block for 100 ticks,
                        so fall through to the normal trap logic below           */
                        pc++;

                    }
                    /* ---- pass syscalls (YIELD/HLT) to kernel dispatcher */
                    mem_write(cpu, 4, in.a);        /* sub-code   (1,2,3) */
                    mem_write(cpu, 5, in.b);        /* operand A  (only PRN) */
                    long handler = mem_read(cpu, 40);       /* syscall vector */
                    if (handler < 0 || handler >= (long)cpu->program_len)
                    DIE("Bad syscall vector");
                    pc = handler;                   /* jump to dispatcher */
                    break;
                }

                default: DIE("Unimplemented opcode");
            }
            /* Commit updated PC */
            mem_write(cpu, 0, pc);
}
