/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavičkový soubor stavového řádku.
 * Spravuje spodní řádek obrazovky – výpis paměti,
 * textový marquee a levé hinty pro editor a file manager.
 */

#ifndef ASTER_STATUSBAR_H
#define ASTER_STATUSBAR_H

#include "types.h"

/** Buffer pro marquee text. */
extern char g_status_marquee[160];
/** 1 pokud je marquee aktivní. */
extern int  g_status_marquee_on;
/** Posun marquee (pro animaci). */
extern usize g_status_marquee_offset;
/** 1 pokud se má zobrazovat jen marquee (žádné paměťové statistiky). */
extern int  g_status_marquee_only;
/** Buffer pro levý hint. */
extern char g_status_left_hint[80];
/** 1 pokud je levý hint aktivní. */
extern int  g_status_left_hint_on;

/** Nastaví text marquee. */
void status_set_marquee(const char *text);
/** Smaže text marquee. */
void status_clear_marquee(void);
/** Nastaví levý hint. */
void status_set_left_hint(const char *text);
/** Smaže levý hint. */
void status_clear_left_hint(void);
/** Vykreslí spodní stavový řádek. */
void render_shell_statusbar(void);

#endif
