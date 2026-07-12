/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor implementuje ovladac PIT casovace.
 * Nastavuje frekvenci hardwaroveho casovace, drzi globalni citac tiku
 * a poskytuje blokujici uspani jadra na zadany pocet milisekund.
 */

#include "timer.h"

static volatile unsigned long g_ticks = 0;

static inline unsigned long long read_tsc(void) {
    unsigned int lo;
    unsigned int hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long)hi << 32) | (unsigned long long)lo;
}

static int interrupts_enabled(void) {
    unsigned long long rflags;
    __asm__ volatile ("pushfq; popq %0" : "=r"(rflags));
    return (rflags & (1ULL << 9)) != 0;
}

static void busy_delay_ms(unsigned long ms) {
    unsigned long long start = read_tsc();
    unsigned long long wait_cycles = (unsigned long long)ms * 1000000ULL;
    unsigned long long end = start + wait_cycles;

    while (read_tsc() < end) {
        __asm__ volatile ("pause");
    }
}

static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

void timer_init(unsigned int hz) {
    unsigned int divisor;

    if (hz == 0) {
        hz = 100;
    }

    divisor = 1193180U / hz;

    outb(0x43, 0x36);
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));
}

unsigned long timer_ticks(void) {
    return g_ticks;
}

void timer_tick_advance(void) {
    ++g_ticks;
}

void timer_sleep_ms(unsigned long ms) {
    if (!interrupts_enabled()) {
        busy_delay_ms(ms);
        return;
    }

    unsigned long start = timer_ticks();
    unsigned long wait_ticks = (ms + 9UL) / 10UL;

    if (wait_ticks == 0) {
        wait_ticks = 1;
    }

    while ((timer_ticks() - start) < wait_ticks) {
        __asm__ volatile ("hlt");
    }
}
