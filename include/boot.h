/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavičkový soubor boot sekvence.
 * Deklaruje inicializační funkci jádra a refresh callback,
 * který je volán z časovače pro obnovení stavového řádku.
 */

#ifndef ASTER_BOOT_H
#define ASTER_BOOT_H

/** Hlavní bootovací sekvence – inicializuje všechny subsystémy. */
void boot_sequence(void);

/** Zpětné volání pro obnovení stavového řádku shellu. */
void shell_status_refresh_callback(void);

#endif
