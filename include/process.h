/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header popisuje procesovy model jadra.
 * Obsahuje stavy procesu, kontext CPU registru a strukturu process_t,
 * plus API pro vytvareni procesu, ukonceni a pristup k procesove tabulce.
 */

#ifndef ASTER_PROCESS_H
#define ASTER_PROCESS_H

#include "types.h"

#define ASTER_MAX_PROCESSES 16

typedef enum {
    PROCESS_UNUSED = 0,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_EXITED
} process_state_t;

typedef struct {
    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 rbx;
    u64 rbp;
    u64 rip;
} context_t;

typedef struct {
    u32 pid;
    process_state_t state;
    u8 priority;
    u8 *stack_base;
    u64 stack_top;
    context_t context;
    const char *name;
} process_t;

void process_init(void);
process_t *process_create(const char *name, void (*entry)(void), u8 priority);
void process_exit(void);
process_t *process_current(void);
void process_set_current(process_t *p);
process_t *process_table(void);
usize process_count(void);

#endif
