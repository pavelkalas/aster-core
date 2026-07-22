/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header vystavuje jednotné API pro textový výstup ve VGA režimu.
 * Definuje operace pro inicializaci, čištění obrazovky, zápis znaku,
 * zápis řetězce a změnu barevné palety textové konzole.
 */

#ifndef ASTER_DISPLAY_H
#define ASTER_DISPLAY_H

#include "types.h"

/** Inicializuje display – nastaví barvu a vyčistí obrazovku. */
void display_init(void);

/** Smaže celou obrazovku a vrátí kurzor na (0,0). */
void display_clear(void);

/** Vypíše jeden znak na obrazovku (zpracovává řídicí znaky). */
void display_putc(char c);

/** Vypíše řetězec na obrazovku. */
void display_write(const char *text);

/** Nastaví barvu popředí a pozadí. */
void display_set_color(u8 fg, u8 bg);

/** Získá aktuální pozici kurzoru. */
void display_get_cursor(usize *out_row, usize *out_col);

/** Nastaví kurzor na zadanou pozici. */
void display_set_cursor(usize new_row, usize new_col);

/** Vyplní celý řádek obrazovky znakem a barvou. */
void display_fill_row(usize target_row, char ch, u8 fg, u8 bg);

/** Zapíše text na zadanou pozici s danou barvou. */
void display_write_at(usize target_row, usize target_col, const char *text, u8 fg, u8 bg);

#endif
