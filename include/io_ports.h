/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavičkový soubor I/O port helpers.
 * Obsahuje funkce pro práci s hardwarovými porty – outb, outw
 * a pomocnou funkci pro čekání na uvolnění KBC bufferu.
 */

#ifndef ASTER_IO_PORTS_H
#define ASTER_IO_PORTS_H

#include "types.h"

/** Zapíše jeden bajt na I/O port. */
void outb(u16 port, u8 value);

/** Zapíše 16bitovou hodnotu na I/O port. */
void outw(u16 port, u16 value);

/** Čeká, dokud není vstupní buffer KBC prázdný. */
int  kbc_wait_input_clear(void);

#endif
