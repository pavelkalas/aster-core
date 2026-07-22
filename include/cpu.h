/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header definuje datový tvar CPU kontextu uloženého při přerušení.
 * Obsahuje strukturu interrupt_frame_t a rozhraní pro inicializaci CPU,
 * nastavení tabulek přerušení a centrální dispatch obsluhy přerušení.
 */

#ifndef ASTER_CPU_H
#define ASTER_CPU_H

#include "types.h"

/** Rámec přerušení – všechny registry uložené při IRQ/výjimce. */
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
    u64 vector;   /**< Číslo vektoru přerušení. */
    u64 error;    /**< Error code (0 pokud není). */
    u64 rip;      /**< Návratová instrukční adresa. */
    u64 cs;       /**< Kódový segment. */
    u64 rflags;   /**< Stavové flagy. */
    u64 rsp;      /**< Ukazatel zásobníku. */
    u64 ss;       /**< Stack segment. */
} interrupt_frame_t;

/** Inicializuje GDT a základní CPU nastavení. */
void cpu_init(void);

/** Inicializuje IDT, PIC a povolí přerušení. */
void interrupts_init(void);

/** Centrální dispatch obsluhy přerušení/výjimek. */
void interrupt_dispatch(interrupt_frame_t *frame);

/** Zastaví CPU (zakáže IRQ, nekonečná HLT smyčka). */
void cpu_halt(void);

#endif
