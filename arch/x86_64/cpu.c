/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento architekturalni modul inicializuje GDT, IDT a remapuje PIC.
 * Obsahuje centralni vstup pro obsluhu vyjimek, hardwarovych preruseni
 * i syscall vektoru, vcetne predani rizeni scheduleru pri timer IRQ.
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

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

struct __attribute__((packed)) gdt_ptr {
    u16 limit;
    u64 base;
};

struct __attribute__((packed)) idt_entry {
    u16 offset_low;
    u16 selector;
    u8 ist;
    u8 type_attr;
    u16 offset_mid;
    u32 offset_high;
    u32 zero;
};

struct __attribute__((packed)) idt_ptr {
    u16 limit;
    u64 base;
};

static u64 gdt_table[3];
static struct idt_entry idt[IDT_ENTRIES];

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

    outb(0x21, 0x00);
    outb(0xA1, 0x00);
}

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

void interrupt_dispatch(interrupt_frame_t *frame) {
    if (frame->vector < 32) {
        printk("[EXC] vector=%d error=%x\n", (int)frame->vector, (u32)frame->error);
        panic("CPU exception");
    }

    if (frame->vector == 32) {
        scheduler_tick();
        outb(0x20, 0x20);
        return;
    }

    if (frame->vector == 33) {
        outb(0x20, 0x20);
        return;
    }

    if (frame->vector == 128) {
        frame->rax = syscall_dispatch(frame->rax, frame->rbx, frame->rcx, frame->rdx, frame->rsi);
        return;
    }
}

void cpu_halt(void) {
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
