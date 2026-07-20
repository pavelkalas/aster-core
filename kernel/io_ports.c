/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 *
 * I/O port helpers – outb, outw, KBC wait.
 */

#include "io_ports.h"

void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

void outw(u16 port, u16 value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

int kbc_wait_input_clear(void) {
    unsigned int i;
    for (i = 0; i < 100000U; ++i) {
        u8 s;
        __asm__ volatile ("inb %1, %0" : "=a"(s) : "Nd"((u16)0x64));
        if ((s & 0x02U) == 0U) {
            return 1;
        }
    }
    return 0;
}
