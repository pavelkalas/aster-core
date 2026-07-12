/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor slouzi jako centralni rozcestnik pro verejne ovladace jadra.
 * Drzi pohromade deklarace klavesnice, casovace a storage vrstvy,
 * aby je kernel mohl jednoduse inicializovat z jednoho mista.
 */

#ifndef ASTER_DRIVERS_H
#define ASTER_DRIVERS_H

#include "serial.h"

#define ASTER_KEY_F1  0x101
#define ASTER_KEY_F2  0x102
#define ASTER_KEY_UP  0x103
#define ASTER_KEY_DOWN 0x104
#define ASTER_KEY_LEFT 0x105
#define ASTER_KEY_RIGHT 0x106

void keyboard_init(void);
void keyboard_set_refresh_callback(void (*cb)(void), unsigned long interval_ticks);
int keyboard_read_key(void);
int keyboard_try_read_key(void);
int keyboard_readline(char *buffer, int max_len);

void timer_init(unsigned int hz);
unsigned long timer_ticks(void);

void storage_init(void);

#endif
