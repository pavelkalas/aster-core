/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header deklaruje fatální panic cestu jádra.
 * Funkce panic je určena pro situace, kdy nelze bezpečně pokračovat,
 * proto vypíše důvod chyby a převede systém do zastaveného stavu.
 */

#ifndef ASTER_PANIC_H
#define ASTER_PANIC_H

/** Zastaví jádro při neobnovitelné chybě a vypíše důvod. */
void panic(const char *reason);

#endif
