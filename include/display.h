/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header vystavuje jednotne API pro textovy vystup v VGA rezimu.
 * Definuje operace pro inicializaci, cisteni obrazovky, zapis znaku,
 * zapis retezce a zmenu barevne palety textove konzole.
 */

#ifndef ASTER_DISPLAY_H
#define ASTER_DISPLAY_H

#include "types.h"

void display_init(void);
void display_clear(void);
void display_putc(char c);
void display_write(const char *text);
void display_set_color(u8 fg, u8 bg);
void display_get_cursor(usize *out_row, usize *out_col);
void display_set_cursor(usize new_row, usize new_col);
void display_fill_row(usize target_row, char ch, u8 fg, u8 bg);
void display_write_at(usize target_row, usize target_col, const char *text, u8 fg, u8 bg);

#endif
