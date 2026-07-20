/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavickovy soubor statusoveho radku.
 * Spravuje spodni radek obrazovky — vypis pameti,
 * textovy marquee a leva hinty pro editor a file manager.
 */

#ifndef ASTER_STATUSBAR_H
#define ASTER_STATUSBAR_H

#include "types.h"

extern char g_status_marquee[160];
extern int  g_status_marquee_on;
extern usize g_status_marquee_offset;
extern int  g_status_marquee_only;
extern char g_status_left_hint[80];
extern int  g_status_left_hint_on;

void status_set_marquee(const char *text);
void status_clear_marquee(void);
void status_set_left_hint(const char *text);
void status_clear_left_hint(void);
void render_shell_statusbar(void);

#endif
