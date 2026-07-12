/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor deklaruje planovac, ktery rozhoduje o prideleni CPU casu.
 * Popisuje vstupni body pro inicializaci, periodicky tick a prepnuti,
 * a funkce pro vyber dalsiho kandidata k vykonu.
 */

#ifndef ASTER_SCHEDULER_H
#define ASTER_SCHEDULER_H

#include "process.h"

void scheduler_init(void);
void scheduler_tick(void);
void scheduler_run_once(void);
void scheduler_yield(void);
process_t *scheduler_pick_next(void);

#endif
