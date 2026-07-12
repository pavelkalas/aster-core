/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor spravuje zivotni cyklus kernel procesu.
 * Implementuje statickou procesovou tabulku, prirazovani PID,
 * pripravu stacku a zakladni operace pro vytvoreni a ukonceni procesu.
 */

#include "memory.h"
#include "process.h"
#include "string.h"

#define PROCESS_STACK_SIZE (16 * 1024)

static process_t g_processes[ASTER_MAX_PROCESSES];
static usize g_process_count = 0;
static u32 g_next_pid = 1;
static process_t *g_current = 0;

void process_init(void) {
    usize i;

    for (i = 0; i < ASTER_MAX_PROCESSES; ++i) {
        g_processes[i].pid = 0;
        g_processes[i].state = PROCESS_UNUSED;
        g_processes[i].priority = 0;
        g_processes[i].stack_base = 0;
        g_processes[i].stack_top = 0;
        g_processes[i].name = 0;
        g_processes[i].context.rip = 0;
    }

    g_process_count = 0;
    g_current = 0;
}

process_t *process_create(const char *name, void (*entry)(void), u8 priority) {
    process_t *p;
    void *stack;

    if (g_process_count >= ASTER_MAX_PROCESSES || !entry) {
        return 0;
    }

    p = &g_processes[g_process_count];
    stack = kmalloc(PROCESS_STACK_SIZE);
    if (!stack) {
        return 0;
    }

    p->pid = g_next_pid++;
    p->state = PROCESS_READY;
    p->priority = priority;
    p->stack_base = (u8 *)stack;
    p->stack_top = (u64)(p->stack_base + PROCESS_STACK_SIZE - 16);
    p->name = name;

    aster_memset(&p->context, 0, sizeof(context_t));
    p->context.rip = (u64)entry;
    p->context.rbp = p->stack_top;

    ++g_process_count;
    return p;
}

void process_exit(void) {
    if (g_current) {
        g_current->state = PROCESS_EXITED;
    }
}

process_t *process_current(void) {
    return g_current;
}

void process_set_current(process_t *p) {
    g_current = p;
}

process_t *process_table(void) {
    return &g_processes[0];
}

usize process_count(void) {
    return g_process_count;
}
