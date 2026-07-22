/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Runtime registr pro sysapps: každá app je mapovaná na příkaz podle
 * názvu souboru a spouští se přes init(void).
 */

#ifndef ASTER_SYSAPPS_RUNTIME_H
#define ASTER_SYSAPPS_RUNTIME_H

/** Typ pro vstupní funkci systémové aplikace. */
typedef void (*sysapp_init_fn_t)(void);

/** Položka registru – mapuje název příkazu na funkci. */
typedef struct {
    const char *name;         /**< Název příkazu. */
    sysapp_init_fn_t entry;   /**< Vstupní funkce aplikace. */
} sysapp_entry_t;

/** Globální tabulka registrovaných sysapps (ukončená NULL entry). */
extern const sysapp_entry_t g_sysapps[];

#endif
