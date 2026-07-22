/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 *
 * I/O port helpers – outb, outw, KBC wait.
 */

#include "io_ports.h"

/**
 * Zapíše jeden bajt na I/O port.
 *
 * @param port  Adresa portu (u16)
 * @param value Hodnota k zápisu (u8)
 */
void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * Zapíše 16bitovou hodnotu na I/O port.
 *
 * @param port  Adresa portu (u16)
 * @param value Hodnota k zápisu (u16)
 */
void outw(u16 port, u16 value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * Čeká, dokud není vstupní buffer klávesnice (KBC) prázdný (bit 2 portu 0x64 je 0).
 *
 * @return 1 pokud se podařilo vyčkat, 0 při timeoutu (int)
 */
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
