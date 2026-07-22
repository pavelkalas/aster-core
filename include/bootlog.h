/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Bootlog — jednotné stavové výpisy, boot stepy a shutdown příprava.
 * Používá se v main.c, drivers, případně dalších částech jádra.
 */

#ifndef ASTER_BOOTLOG_H
#define ASTER_BOOTLOG_H

#include "types.h"

/** Přidá text na konec bufferu. */
usize append_text(char *dst, usize pos, usize max, const char *src);
/** Přidá unsigned int jako dekadický text na konec bufferu. */
usize append_uint(char *dst, usize pos, usize max, unsigned int v);

/** Zjistí, zda je boot splash aktivní. */
int  boot_splash_is_active(void);
/** Resetuje splash pro daný počet kroků. */
void boot_splash_reset(unsigned int steps_total);
/** Dokončí splash obrazovku. */
void boot_splash_finish(void);

/** Vypíše stavovou zprávu [ STAV ] msg. */
void bootlog_state(const char *state, unsigned char state_color, const char *msg);
/** Vypíše [ HOTOVO ] msg. */
void bootlog_ok(const char *msg);
/** Vypíše [ CHYBA ] msg. */
void bootlog_error(const char *msg);

/** Zahájí boot krok. */
void boot_step_begin(const char *label);
/** Dokončí boot krok s daným stavem a barvou. */
void boot_step_finish(const char *label, const char *state, u8 state_color);
/** Dokončí boot krok s OK stavem. */
void boot_step_ok(const char *label);
/** Dokončí boot krok s SKIP stavem. */
void boot_step_skip(const char *label);

/** Připraví systém na vypnutí/restart. */
void system_shutdown_prepare(const char *typ);

#endif
