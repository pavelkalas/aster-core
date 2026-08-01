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

/** Softwarovy interrupt pouzity pro navrat z ring 3 do kernelu. */
#define RING3_RETURN_VECTOR 129

/** Inicializace ring 3 (TSS, syscall MSR). */
void ring3_init(void);

/**
 * Spusti sysapp vstupni funkci v ring 3 a po dokonceni se vrati do kernelu.
 *
 * @param entry Ukazatel na vstupni funkci sysapp (void (*)(void))
 * @return      0 pri uspechu, jinak zaporna chyba
 */
int ring3_run_sysapp(void (*entry)(void));

/**
 * Obsluha user interruptu pro navrat z ring 3 do kernelu.
 *
 * @return 1 pokud byl vektor zpracovan jako ring3 navrat, jinak 0.
 */
int ring3_handle_return_vector(u64 vector,
                               u64 cs,
                               u64 *cs_out,
                               u64 *rip,
                               u64 *rsp,
                               u64 *rflags,
                               u64 *ss,
                               u64 *rbp);

/**
 * Vstoupí do ring 3 (uživatelský mód) na zadané adrese.
 *
 * @param entry_point Adresa vstupního bodu v ring 3 (u64)
 * @param user_stack  Vrchol uživatelského zásobníku (u64)
 */
void enter_ring3(u64 entry_point, u64 user_stack);

#endif
