/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavickovy soubor shellu.
 * Deklaruje hlavni shell smycku a pomocne funkce,
 * ktere potrebuji ostatni moduly (chybove hlasky, banner, aliases).
 */

#ifndef ASTER_SHELL_H
#define ASTER_SHELL_H

void shell_loop(void);
void show_aster_banner(const char *subtitle);
void ensure_aliases_file(void);
void print_error(const char *msg);

#endif
