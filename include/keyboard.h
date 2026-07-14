/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor deklaruje rozhrani pro PS/2 klavesnici.
 * Poskytuje funkce pro cteni stisknutych klaves a nacitani
 * celych radku textu od uzivatele.
 */

 #ifndef KEYBOARD_H
#define KEYBOARD_H

int keyboard_readline(char *buffer, int max_len);

#endif
