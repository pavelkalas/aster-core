/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Ovladač sériového portu (COM1).
 * Inicializuje sériovou komunikaci a umožňuje
 * odesílat znaky a text přes sériový port.
 */

#include "serial.h"

#define COM1_PORT 0x3F8

static int g_serial_ready = 0;

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

int serial_is_ready(void) {
    return g_serial_ready;
}

void serial_init(void) {
    g_serial_ready = 0;

    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x03);
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0B);

    outb(COM1_PORT + 4, 0x1E);
    outb(COM1_PORT + 0, 0xAE);
    if (inb(COM1_PORT + 0) != 0xAE) {
        return;
    }

    outb(COM1_PORT + 4, 0x0F);
    g_serial_ready = 1;
}

void serial_write_char(char c) {
    u32 spin;

    if (!g_serial_ready) {
        return;
    }

    if (c == '\n') {
        serial_write_char('\r');
    }

    for (spin = 0; spin < 100000U; ++spin) {
        if ((inb(COM1_PORT + 5) & 0x20U) != 0U) {
            outb(COM1_PORT + 0, (u8)c);
            return;
        }
    }
}

void serial_write_string(const char *text) {
    if (!text) {
        return;
    }

    while (*text) {
        serial_write_char(*text++);
    }
}
