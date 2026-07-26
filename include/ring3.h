/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Ring 3 – hlavičkový soubor pro vstup do uživatelského módu.
 */

#ifndef ASTER_RING3_H
#define ASTER_RING3_H

#include "types.h"

/** Inicializace ring 3 (TSS, syscall MSR). */
void ring3_init(void);

/**
 * Vstoupí do ring 3 (uživatelský mód) na zadané adrese.
 *
 * @param entry_point Adresa vstupního bodu v ring 3 (u64)
 * @param user_stack  Vrchol uživatelského zásobníku (u64)
 */
void enter_ring3(u64 entry_point, u64 user_stack);

#endif
