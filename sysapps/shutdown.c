/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: shutdown – Vypne systém.
 * Použití: shutdown
 */

#include "aster_api.h"

void init(void) {
    aster_api_shutdown();
}
