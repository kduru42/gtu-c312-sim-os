/*  simulation.c
 *  ------------------------------------------------------------------
 *  Usage:
 *      ./simulate  program.txt  -D <level>
 *
 *      level 0 : no per-tick output; dump full memory when CPU halts
 *      level 1 : dump full memory after every instruction
 *      level 2 : like 1, but wait for ENTER after each dump            (single-step)
 *      level 3 : dump thread-table after every context-switch/syscall
 *
 *  Build  (assuming cpu.c + cpu.h are in the same directory)
 *      gcc -std=c11 -Wall -Wextra -Werror cpu.c simulation.c -o simulate
 */
#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

FILE *log_cpu = NULL; // simulated as error stream to avoid missing information

static int debug_mode = 0;          /* 0-3 */

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */
static void dump_memory(const Cpu *cpu)
{
    fprintf(log_cpu, "──────── MEMORY DUMP ────────\n");
    for (long i = 0; i < 4000; ++i) // limited with 4000 because of performance issue
        fprintf(log_cpu, "%4ld : %ld\n", i, cpu->memory[i]);
}

void dump_thread_table(const Cpu *c)
{
    const long base = 100, size = 8;
    fprintf(log_cpu, "ID |   PC   SP  ST  BLK  START  USED\n");
    for (int i=0;i<11;++i){
        long b = base + i*size;
        long id   = c->memory[b+0];
        if(id==0 && i!=0) continue;  /* empty slot */
        fprintf(log_cpu ,"%2ld | %4ld %4ld  %2ld  %4ld  %5ld  %5ld\n",
               id,
               c->memory[b+3],      /* saved-PC   */
               c->memory[b+4],      /* saved-SP   */
               c->memory[b+5],      /* state      */
               c->memory[b+6],      /* block ctr  */
               c->memory[b+1],      /* start time */
               c->memory[b+2]);     /* instr used */
    }
}

/* ------------------------------------------------------------------ */
/*  Detect “interesting events” (syscall / context-switch)            */
/* ------------------------------------------------------------------ */
static int last_thread_id = -1;
static void debug_hook_before(const Cpu *cpu)
{
    if (debug_mode == 3) {
        int cur = (int)cpu->memory[11];   /* CUR_THREAD cell in OS */
        long sysno = cpu->memory[4];      /* syscall code */
        if (cpu->memory[0] == 87) // syscall
        {
            if (cur != last_thread_id || (sysno >= 0 && sysno <= 2)) {
                dump_thread_table(cpu);
                last_thread_id = cur;
            }
        }
    }
}

static void debug_hook_after(const Cpu *cpu)
{
    if (debug_mode == 1 || debug_mode == 2)
        dump_memory(cpu);

    if (debug_mode == 2) {
        fputs("Press ENTER to continue...", stderr);
        char dummy_char;
        read(STDIN_FILENO, &dummy_char, 1);
    }
}

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s program.txt [-D 0|1|2|3]\n", argv[0]);
        return 1;
    }
    if (argc >= 4 && strcmp(argv[2], "-D") == 0)
        debug_mode = atoi(argv[3]);

    log_cpu = fopen(filename, "w");
    if (!log_cpu) { perror(filename); exit(EXIT_FAILURE); }

    Cpu cpu;
    if (cpu_init(&cpu) < 0)                return 1;
    if (cpu_load_file(&cpu, argv[1]) < 0)  return 1;

    /* ------------------ simulation loop ------------------ */
    while (!cpu_is_halted(&cpu)) {
        debug_hook_before(&cpu);     /* context-switch / syscall trace  */
        cpu_execute(&cpu);           /* one tick                        */
        debug_hook_after(&cpu);      /* mem dump / single-step          */
    }

    /* final full dump for mode 0 (or leave it for all modes) */
    if (debug_mode == 0)
        dump_memory(&cpu);

    printf("CPU Halted\n");
    fclose(log_cpu);
    return 0;
}
