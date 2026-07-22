/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor deklaruje plánovač, který rozhoduje o přidělení CPU času.
 * Popisuje vstupní body pro inicializaci, periodický tick a přepnutí,
 * a funkce pro výběr dalšího kandidáta k vykonu.
 */

#ifndef ASTER_SCHEDULER_H
#define ASTER_SCHEDULER_H

#include "process.h"

/** Inicializuje plánovač. */
void scheduler_init(void);

/** Zpracuje tik plánovače (voláno z IRQ0). */
void scheduler_tick(void);

/** Provede jeden běh plánovače (vybere a spustí další proces). */
void scheduler_run_once(void);

/** Dobrovolně předá řízení plánovači (yield). */
void scheduler_yield(void);

/** Vybere další proces k běhu (round-robin). */
process_t *scheduler_pick_next(void);

#endif
