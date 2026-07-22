/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor slouží jako centrální rozcestník pro veřejné ovladače jádra.
 * Drží pohromadě deklarace klávesnice, časovače a storage vrstvy,
 * aby je kernel mohl jednoduše inicializovat z jednoho místa.
 */

#ifndef ASTER_DRIVERS_H
#define ASTER_DRIVERS_H

#include "serial.h"

/** Konstanty pro speciální klávesy (F1, F2, šipky). */
#define ASTER_KEY_F1  0x101
#define ASTER_KEY_F2  0x102
#define ASTER_KEY_UP  0x103
#define ASTER_KEY_DOWN 0x104
#define ASTER_KEY_LEFT 0x105
#define ASTER_KEY_RIGHT 0x106

/** Inicializuje klávesnici (PS/2). */
void keyboard_init(void);

/** Nastaví callback pro periodické obnovení obrazovky. */
void keyboard_set_refresh_callback(void (*cb)(void), unsigned long interval_ticks);

/** Přečte jednu klávesu (blokující). */
int keyboard_read_key(void);

/** Přečte klávesu (neblokující). */
int keyboard_try_read_key(void);

/** Přečte řádek textu z klávesnice. */
int keyboard_readline(char *buffer, int max_len);

/** Inicializuje PIT časovač na danou frekvenci. */
void timer_init(unsigned int hz);

/** Vrátí počet tiků časovače od startu. */
unsigned long timer_ticks(void);

/** Inicializuje storage (AsterFS nad ATA diskem). */
void storage_init(void);

#endif
