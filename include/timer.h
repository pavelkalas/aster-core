/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor deklaruje casovaci vrstvu nad PIT hardwarem.
 * Zahrnuje inicializaci frekvence, cteni tiku, inkrement z ISR
 * a blokujici uspani jadra na zadany pocet milisekund.
 */

#ifndef ASTER_TIMER_H
#define ASTER_TIMER_H

void timer_init(unsigned int hz);
unsigned long timer_ticks(void);
void timer_tick_advance(void);
void timer_sleep_ms(unsigned long ms);

#endif
