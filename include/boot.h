/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavickovy soubor boot sekvence.
 * Deklaruje inicializacni funkci jadra a refresh callback,
 * ktery je volany z casovace pro obnoveni stavoveho radku.
 */

#ifndef ASTER_BOOT_H
#define ASTER_BOOT_H

void boot_sequence(void);
void shell_status_refresh_callback(void);

#endif
