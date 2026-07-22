/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor definuje rozhraní pro kernelový diagnostický výstup.
 * Poskytuje jednoduché API pro výpis řetězce i formátovaný log,
 * který slouží pro ladění bootu, přerušení i běhu kernel služeb.
 */

#ifndef ASTER_PRINTK_H
#define ASTER_PRINTK_H

/** Vypíše řetězec na obrazovku (bez formátování). */
void aster_print(const char *text);

/** Formátovaný výstup (podpora %s, %u, %d, %p, %x, %c). */
void printk(const char *fmt, ...);

#endif
