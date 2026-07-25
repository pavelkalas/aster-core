/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: clear – Vyčistí obrazovku.
 * Použití: clear
 */

#include "aster_api.h"

void init(void) {
    aster_api_clear();
}
