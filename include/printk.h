/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor definuje rozhrani pro kernelovy diagnosticky vystup.
 * Poskytuje jednoduche API pro vypis retezce i formatovany log,
 * ktery slouzi pro ladeni bootu, preruseni i behu kernel sluzeb.
 */

#ifndef ASTER_PRINTK_H
#define ASTER_PRINTK_H

void aster_print(const char *text);
void printk(const char *fmt, ...);

#endif
