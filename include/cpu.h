/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header definuje datovy tvar CPU kontextu ulozeneho pri preruseni.
 * Obsahuje strukturu interrupt_frame_t a rozhrani pro inicializaci CPU,
 * nastaveni tabulek preruseni a centralni dispatch obsluhy preruseni.
 */

#ifndef ASTER_CPU_H
#define ASTER_CPU_H

#include "types.h"

typedef struct {
    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;
    u64 rsi;
    u64 rdi;
    u64 rbp;
    u64 rdx;
    u64 rcx;
    u64 rbx;
    u64 rax;
    u64 vector;
    u64 error;
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
} interrupt_frame_t;

void cpu_init(void);
void interrupts_init(void);
void interrupt_dispatch(interrupt_frame_t *frame);
void cpu_halt(void);

#endif
