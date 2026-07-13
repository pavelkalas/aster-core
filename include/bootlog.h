/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Header deklaruje jednoduche API pro jednotne stavove vypisy
 * ve stylu [ STAV ] zprava, pouzivane hlavne pri boot/setup toku.
 */

#ifndef ASTER_BOOTLOG_H
#define ASTER_BOOTLOG_H

void bootlog_state(const char *state, unsigned char state_color, const char *msg);
void bootlog_ok(const char *msg);
void bootlog_error(const char *msg);

#endif