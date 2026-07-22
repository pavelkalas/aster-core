/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor deklaruje časovací vrstvu nad PIT hardwarem.
 * Zahrnuje inicializaci frekvence, čtení tiků, inkrement z ISR
 * a blokující uspání jádra na zadaný počet milisekund.
 */

#ifndef ASTER_TIMER_H
#define ASTER_TIMER_H

/** Inicializuje PIT časovač na danou frekvenci (Hz). */
void timer_init(unsigned int hz);

/** Vrátí aktuální počet tiků od startu systému. */
unsigned long timer_ticks(void);

/** Posune čítač tiků o 1 (voláno z obsluhy přerušení). */
void timer_tick_advance(void);

/** Zablokuje běh na zadaný počet milisekund. */
void timer_sleep_ms(unsigned long ms);

#endif
