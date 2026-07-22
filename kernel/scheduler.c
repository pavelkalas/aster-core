/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento modul implementuje jednoduchý plánovač typu round-robin.
 * Reaguje na tiky časovače, vybírá další běžící proces
 * a řídí přepínání mezi stavy READY, RUNNING a EXITED.
 */

#include "process.h"
#include "scheduler.h"
#include "timer.h"

static usize g_current_index = 0;
static unsigned long g_quantum_ticks = 0;

/**
 * Inicializuje plánovač – vynuluje index a čítač kvanta.
 */
void scheduler_init(void) {
    g_current_index = 0;
    g_quantum_ticks = 0;
}

/**
 * Vybere další proces k běhu (round-robin) z procesové tabulky.
 * Hledá proces ve stavu READY nebo RUNNING.
 *
 * @return Ukazatel na vybraný proces, nebo NULL pokud žádný není (process_t *)
 */
process_t *scheduler_pick_next(void) {
    process_t *table = process_table();
    usize count = process_count();
    usize i;

    if (count == 0) {
        return 0;
    }

    for (i = 0; i < count; ++i) {
        usize idx = (g_current_index + i + 1) % count;
        if (table[idx].state == PROCESS_READY || table[idx].state == PROCESS_RUNNING) {
            g_current_index = idx;
            return &table[idx];
        }
    }

    return 0;
}

/**
 * Provede jeden běh plánovače – vybere další proces a spustí ho.
 * Pokud je proces ve stavu READY, spustí jeho vstupní funkci.
 */
void scheduler_run_once(void) {
    process_t *next = scheduler_pick_next();

    if (!next) {
        return;
    }

    process_set_current(next);
    if (next->state == PROCESS_READY) {
        void (*entry)(void) = (void (*)(void))next->context.rip;
        next->state = PROCESS_RUNNING;
        entry();
        if (next->state == PROCESS_RUNNING) {
            next->state = PROCESS_EXITED;
        }
    }
}

/**
 * Dobrovolně předá řízení plánovači (yield).
 * Aktuální proces přepne do READY a spustí se další.
 */
void scheduler_yield(void) {
    process_t *current = process_current();

    if (current && current->state == PROCESS_RUNNING) {
        current->state = PROCESS_READY;
    }

    scheduler_run_once();
}

/**
 * Zpracuje tik plánovače – každý 5. tik provede přepnutí procesu.
 */
void scheduler_tick(void) {
    timer_tick_advance();
    ++g_quantum_ticks;

    if ((g_quantum_ticks % 5) == 0) {
        scheduler_run_once();
    }
}
