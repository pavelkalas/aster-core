/*
 * AsterOS Kernel
 * Serial (COM1) diagnostics output
 * Autor: Pavel Kalaš
 * Rok: 2026
 */

/*
 * Hlavičkový soubor sériového ovladače (COM1).
 * Poskytuje funkce pro inicializaci a výstup textu
 * přes sériový port pro diagnostické účely.
 */

#ifndef ASTER_SERIAL_H
#define ASTER_SERIAL_H

#include "types.h"

/** Inicializuje sériový port COM1. */
void serial_init(void);

/** Zjistí, zda je sériový port připraven. */
int serial_is_ready(void);

/** Odešle jeden znak přes sériový port. */
void serial_write_char(char c);

/** Odešle řetězec přes sériový port. */
void serial_write_string(const char *text);

#endif
