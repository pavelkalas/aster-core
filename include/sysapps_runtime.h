/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Runtime registr pro sysapps: každá app je mapovaná na příkaz podle
 * názvu souboru a spouští se přes init(void).
 *
 * Podpora argumentů: shell nastaví g_sysapp_argc/g_sysapp_argv
 * před spuštěním aplikace. Aplikace je čte přes aster_api_get_argc/argv.
 */

#ifndef ASTER_SYSAPPS_RUNTIME_H
#define ASTER_SYSAPPS_RUNTIME_H

#include "types.h"

/** Maximální počet argumentů pro sysapp. */
#define SYSAPP_MAX_ARGS 16

/** Typ pro vstupní funkci systémové aplikace. */
typedef void (*sysapp_init_fn_t)(void);

/** Položka registru – mapuje název příkazu na funkci. */
typedef struct {
    const char *name;         /**< Název příkazu. */
    sysapp_init_fn_t entry;   /**< Vstupní funkce aplikace. */
} sysapp_entry_t;

/** Globální tabulka registrovaných sysapps (ukončená NULL entry). */
extern const sysapp_entry_t g_sysapps[];

/** Argumenty předané z shellu – nastavuje shell před voláním sysapp. */
extern int   g_sysapp_argc;
extern char *g_sysapp_argv[SYSAPP_MAX_ARGS];

/** Příznak, že shell má ukončit smyčku (pro příkaz exit/logout). */
extern int g_shell_should_exit;

/** Vrátí počet argumentů předaných sysapp. */
int sysapp_get_argc(void);

/** Vrátí argument na daném indexu (nebo NULL). */
const char *sysapp_get_argv(int i);

#endif
