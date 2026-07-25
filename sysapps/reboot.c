/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: reboot – Restartuje systém.
 * Použití: reboot
 */

#include "aster_api.h"

void init(void) {
    aster_api_reboot();
}
