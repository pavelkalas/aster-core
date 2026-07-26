/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header definuje datový tvar CPU kontextu uloženého při přerušení.
 * Obsahuje strukturu interrupt_frame_t, TSS pro ring 3 přechody
 * a rozhraní pro inicializaci CPU, nastavení tabulek přerušení
 * a centrální dispatch obsluhy přerušení.
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

/**
 * 64bitový Task State Segment (TSS) pro x86_64.
 * Používá se pro přepínání zásobníku při přechodu z ring 3 do ring 0.
 */
typedef struct __attribute__((packed)) {
    u32 reserved0;
    u64 rsp0;      /**< Kernel stack pointer (ring 0). */
    u64 rsp1;      /**< Stack pro ring 1 (nepoužito). */
    u64 rsp2;      /**< Stack pro ring 2 (nepoužito). */
    u64 reserved1;
    u64 ist1;      /**< Interrupt Stack Table 1. */
    u64 ist2;      /**< Interrupt Stack Table 2. */
    u64 ist3;      /**< Interrupt Stack Table 3. */
    u64 ist4;      /**< Interrupt Stack Table 4. */
    u64 ist5;      /**< Interrupt Stack Table 5. */
    u64 ist6;      /**< Interrupt Stack Table 6. */
    u64 ist7;      /**< Interrupt Stack Table 7. */
    u64 reserved2;
    u16 reserved3;
    u16 iopb;      /**< Offset I/O Permission Bitmap. */
} tss_t;

/*
 * GDT selektory segmentů.
 */
#define GDT_KERNEL_CODE  0x08  /**< Kernel code segment (ring 0). */
#define GDT_KERNEL_DATA  0x10  /**< Kernel data segment (ring 0). */
#define GDT_USER_DATA    0x1B  /**< User data segment (ring 3, 0x18 | RPL3). */
#define GDT_USER_CODE    0x23  /**< User code segment (ring 3, 0x20 | RPL3). */

/** Inicializuje GDT, TSS a základní CPU nastavení. */
void cpu_init(void);

/** Inicializuje IDT, PIC a povolí přerušení. */
void interrupts_init(void);

/** Centrální dispatch obsluhy přerušení/výjimek. */
void interrupt_dispatch(interrupt_frame_t *frame);

/** Zastaví CPU (zakáže IRQ, nekonečná HLT smyčka). */
void cpu_halt(void);

/** Nastaví kernel stack pointer (RSP0) v TSS pro ring 3 přechody. */
void tss_set_rsp0(u64 rsp0);

/** Vrátí ukazatel na TSS. */
tss_t *tss_get(void);

/** Načte TSS segment registr (TR). */
void tss_flush(void);

#endif
