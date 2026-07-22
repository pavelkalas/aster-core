/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento architekturální modul inicializuje GDT, IDT a remapuje PIC.
 * Obsahuje centrální vstup pro obsluhu výjimek, hardwarových přerušení
 * i syscall vektoru, včetně předání řízení scheduleru při timer IRQ.
 */

#include "cpu.h"
#include "panic.h"
#include "printk.h"
#include "scheduler.h"
#include "syscall.h"
#include "timer.h"
#include "types.h"

extern void arch_load_gdt(void *ptr);
extern void arch_reload_segments(void);
extern void arch_load_idt(void *ptr);
extern void *isr_stub_table[];

#define IDT_ENTRIES 256

/** Zapíše bajt na I/O port. */
static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/** Krátká I/O pauza pro synchronizaci. */
static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

/** Struktura ukazatele na GDT. */
struct __attribute__((packed)) gdt_ptr {
    u16 limit;
    u64 base;
};

/** Položka IDT – 16bajtová struktura (x86_64). */
struct __attribute__((packed)) idt_entry {
    u16 offset_low;
    u16 selector;
    u8 ist;
    u8 type_attr;
    u16 offset_mid;
    u32 offset_high;
    u32 zero;
};

/** Ukazatel na IDT (pro lidt instrukci). */
struct __attribute__((packed)) idt_ptr {
    u16 limit;
    u64 base;
};

static u64 gdt_table[3];
static struct idt_entry idt[IDT_ENTRIES];

/**
 * Nastaví IDT gate pro daný vektor.
 *
 * @param vec   Číslo vektoru (u8)
 * @param isr   Ukazatel na obsluhu (void *)
 * @param flags Atributy (typ) (u8)
 */
static void set_idt_gate(u8 vec, void *isr, u8 flags) {
    u64 addr = (u64)isr;
    idt[vec].offset_low = (u16)(addr & 0xFFFF);
    idt[vec].selector = 0x08;
    idt[vec].ist = 0;
    idt[vec].type_attr = flags;
    idt[vec].offset_mid = (u16)((addr >> 16) & 0xFFFF);
    idt[vec].offset_high = (u32)((addr >> 32) & 0xFFFFFFFF);
    idt[vec].zero = 0;
}

/**
 * Remapuje PIC (Programmable Interrupt Controller) z výchozích vektorů
 * (0–15) na vektory 32–47, aby se předešlo kolizi s CPU výjimkami.
 * Povoluje IRQ0 (časovač), IRQ1 (klávesnice) a IRQ2 (kaskáda).
 */
static void pic_remap(void) {
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();

    outb(0x21, 0x20);
    io_wait();
    outb(0xA1, 0x28);
    io_wait();

    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();

    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    /* Povolit IRQ0 (timer), IRQ1 (keyboard), IRQ2 (cascade); ostatní maskovat */
    outb(0x21, 0xF8);
    outb(0xA1, 0xFF);
}

/**
 * Přečte hodnotu CR2 registru (obsahuje adresu při Page Fault).
 *
 * @return Hodnota CR2 (u64)
 */
static inline u64 read_cr2(void) {
    u64 value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

/**
 * Odešle EOI (End Of Interrupt) signál PIC.
 *
 * @param vector Číslo vektoru (u64)
 */
static void pic_send_eoi(u64 vector) {
    if (vector >= 40) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}

/**
 * Vrátí název výjimky podle čísla vektoru.
 *
 * @param vector Číslo vektoru (u64)
 * @return       Textový název výjimky (const char *)
 */
static const char *exception_name(u64 vector) {
    switch (vector) {
        case 8:
            return "Double Fault";
        case 13:
            return "General Protection Fault";
        case 14:
            return "Page Fault";
        default:
            return "CPU Exception";
    }
}

/**
 * Vypíše obsah interrupt_frame_t (registry, RIP, RSP, atd.) při výjimce.
 *
 * @param f Ukazatel na frame (interrupt_frame_t *)
 */
static void dump_frame(interrupt_frame_t *f) {
    printk("[EXC] %s (vector=%u error=%x)\n", exception_name(f->vector), (u32)f->vector, (u32)f->error);
    printk("[EXC] RIP=%p CS=%p RFLAGS=%p RSP=%p SS=%p\n", (void *)f->rip, (void *)f->cs, (void *)f->rflags, (void *)f->rsp, (void *)f->ss);
    printk("[EXC] RAX=%p RBX=%p RCX=%p RDX=%p\n", (void *)f->rax, (void *)f->rbx, (void *)f->rcx, (void *)f->rdx);
    printk("[EXC] RSI=%p RDI=%p RBP=%p\n", (void *)f->rsi, (void *)f->rdi, (void *)f->rbp);
    printk("[EXC] R8 =%p R9 =%p R10=%p R11=%p\n", (void *)f->r8, (void *)f->r9, (void *)f->r10, (void *)f->r11);
    printk("[EXC] R12=%p R13=%p R14=%p R15=%p\n", (void *)f->r12, (void *)f->r13, (void *)f->r14, (void *)f->r15);

    if (f->vector == 14) {
        printk("[EXC] CR2(fault addr)=%p\n", (void *)read_cr2());
    }
}

/**
 * Inicializuje GDT (Global Descriptor Table).
 * Nastavuje null descriptor, kódový segment (ring 0) a datový segment (ring 0).
 */
void cpu_init(void) {
    struct gdt_ptr gp;

    gdt_table[0] = 0x0000000000000000ULL;
    gdt_table[1] = 0x00AF9A000000FFFFULL;
    gdt_table[2] = 0x00CF92000000FFFFULL;

    gp.limit = sizeof(gdt_table) - 1;
    gp.base = (u64)&gdt_table[0];

    arch_load_gdt(&gp);
    arch_reload_segments();
}

/**
 * Inicializuje IDT a PIC.
 * Nastaví obsluhy pro vektory 0–33 (výjimky + IRQ0–IRQ1) a vektor 128 (syscall).
 * Poté povolí přerušení (STI).
 */
void interrupts_init(void) {
    struct idt_ptr ip;
    u32 i;

    for (i = 0; i < IDT_ENTRIES; ++i) {
        set_idt_gate((u8)i, isr_stub_table[0], 0x8E);
    }

    for (i = 0; i < 34; ++i) {
        set_idt_gate((u8)i, isr_stub_table[i], 0x8E);
    }

    set_idt_gate(128, isr_stub_table[34], 0xEE);

    ip.limit = sizeof(idt) - 1;
    ip.base = (u64)&idt[0];

    arch_load_idt(&ip);
    pic_remap();

    __asm__ volatile ("sti");
}

/**
 * Hlavní dispatcher přerušení a výjimek.
 * Volá se z assembly stubů po uložení registrů.
 *
 * @param frame Ukazatel na interrupt frame (interrupt_frame_t *)
 */
void interrupt_dispatch(interrupt_frame_t *frame) {
    if (frame->vector < 32) {
        dump_frame(frame);
        panic("CPU exception");
    }

    if (frame->vector == 32) {
        scheduler_tick();
        pic_send_eoi(frame->vector);
        return;
    }

    if (frame->vector == 33) {
        pic_send_eoi(frame->vector);
        return;
    }

    if (frame->vector == 128) {
        frame->rax = syscall_dispatch(frame->rax, frame->rbx, frame->rcx, frame->rdx, frame->rsi);
        return;
    }
}

/**
 * Zastaví CPU (zakáže přerušení a vstoupí do nekonečné smyčky HLT).
 */
void cpu_halt(void) {
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
