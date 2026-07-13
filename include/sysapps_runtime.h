/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Runtime registr pro sysapps: kazda app je mapovana na prikaz podle
 * nazvu souboru a spousti se pres init(void).
 */

#ifndef ASTER_SYSAPPS_RUNTIME_H
#define ASTER_SYSAPPS_RUNTIME_H

typedef void (*sysapp_init_fn_t)(void);

typedef struct {
    const char *name;
    sysapp_init_fn_t entry;
} sysapp_entry_t;

extern const sysapp_entry_t g_sysapps[];

#endif
