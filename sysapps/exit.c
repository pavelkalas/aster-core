/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: exit – Odhlásí aktuálního uživatele.
 * Použití: exit
 */

#include "aster_api.h"

void init(void) {
    aster_api_shell_exit();
}
