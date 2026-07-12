/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header popisuje syscall ABI mezi user casti a jadrem.
 * Definuje cisla volani, centralni dispatcher a obalove funkce,
 * ktere mapuji uzivatelske pozadavky na kernelove sluzby.
 */

#ifndef ASTER_SYSCALL_H
#define ASTER_SYSCALL_H

#include "types.h"

#define SYSCALL_WRITE          0
#define SYSCALL_ALLOC          1
#define SYSCALL_PROCESS_CREATE 2

void syscall_init(void);
u64 syscall_dispatch(u64 id, u64 a1, u64 a2, u64 a3, u64 a4);

u64 aster_write(const char *text);
void *aster_alloc(usize size);
i64 aster_process_create(void (*entry)(void), const char *name, u8 priority);

#endif
