/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header popisuje procesový model jádra.
 * Obsahuje stavy procesu, kontext CPU registrů a strukturu process_t,
 * plus API pro vytváření procesů, ukončení a přístup k procesové tabulce.
 */

#ifndef ASTER_PROCESS_H
#define ASTER_PROCESS_H

#include "types.h"

/** Maximální počet procesů v tabulce. */
#define ASTER_MAX_PROCESSES 16

/** Stavy procesů. */
typedef enum {
    PROCESS_UNUSED = 0,   /**< Místo v tabulce je neobsazeno. */
    PROCESS_READY,        /**< Proces je připraven k běhu. */
    PROCESS_RUNNING,      /**< Proces právě běží. */
    PROCESS_BLOCKED,      /**< Proces je blokován. */
    PROCESS_EXITED        /**< Proces skončil. */
} process_state_t;

/** Kontext procesu – uložené registry při přepnutí. */
typedef struct {
    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 rbx;
    u64 rbp;
    u64 rip;
} context_t;

/** Struktura procesu. */
typedef struct {
    u32 pid;                  /**< Identifikátor procesu. */
    process_state_t state;    /**< Aktuální stav. */
    u8 priority;              /**< Priorita (0–255). */
    u8 *stack_base;           /**< Základ zásobníku. */
    u64 stack_top;            /**< Vrchol zásobníku. */
    context_t context;        /**< Uložený kontext registrů. */
    const char *name;         /**< Název procesu. */
} process_t;

/** Inicializuje procesovou tabulku. */
void process_init(void);

/** Vytvoří nový proces. */
process_t *process_create(const char *name, void (*entry)(void), u8 priority);

/** Ukončí aktuální proces. */
void process_exit(void);

/** Vrátí aktuálně běžící proces. */
process_t *process_current(void);

/** Nastaví aktuální proces. */
void process_set_current(process_t *p);

/** Vrátí ukazatel na procesovou tabulku. */
process_t *process_table(void);

/** Vrátí počet procesů v tabulce. */
usize process_count(void);

#endif
