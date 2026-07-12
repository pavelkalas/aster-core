/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header deklaruje fatalni panic cestu jadra.
 * Funkce panic je urcena pro situace, kdy nelze bezpecne pokracovat,
 * proto vypise duvod chyby a prevede system do zastaveneho stavu.
 */

#ifndef ASTER_PANIC_H
#define ASTER_PANIC_H

void panic(const char *reason);

#endif
