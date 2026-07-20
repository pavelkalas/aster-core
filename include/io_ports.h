/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavickovy soubor I/O port helpers.
 * Obsahuje funkce pro praci s hardwarovymi porty — outb, outw
 * a pomocnou funkci pro cekani na uvolneni KBC bufferu.
 */

#ifndef ASTER_IO_PORTS_H
#define ASTER_IO_PORTS_H

#include "types.h"

void outb(u16 port, u8 value);
void outw(u16 port, u16 value);
int  kbc_wait_input_clear(void);

#endif
