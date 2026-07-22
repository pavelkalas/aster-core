/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavičkový soubor pro parsování celých čísel z řetězců.
 * Obsahuje funkce pro převod řetězce na unsigned long a signed long.
 */

#ifndef ASTER_INT_H
#define ASTER_INT_H

/** Převede řetězec na unsigned long (dekadicky). */
unsigned long parse_u32(const char *s, int *ok);

/** Převede řetězec na signed long (dekadicky, podporuje '-'). */
long parse_i32(const char *s, int *ok);

#endif
