/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor deklaruje rozhraní pro PS/2 klávesnici.
 * Poskytuje funkce pro čtení stisknutých kláves a načítání
 * celých řádků textu od uživatele.
 */

 #ifndef KEYBOARD_H
#define KEYBOARD_H

/** Přečte řádek textu z klávesnice (s historií a refresh callbackem). */
int keyboard_readline(char *buffer, int max_len);

#endif
