/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavičkový soubor shellu.
 * Deklaruje hlavní shell smyčku a pomocné funkce,
 * které potřebují ostatní moduly (chybové hlášky, banner, aliases).
 */

#ifndef ASTER_SHELL_H
#define ASTER_SHELL_H

/** Hlavní smyčka shellu – čte a zpracovává příkazy. */
void shell_loop(void);

/** Zobrazí banner AsterOS s volitelným podtitulem. */
void show_aster_banner(const char *subtitle);

/** Vypíše chybovou hlášku červeně. */
void print_error(const char *msg);

#endif
