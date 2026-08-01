/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: ring3
 * Pouziti: ring3
 *
 * Informacni prikaz - vsechny sysapps uz bezne bezi v ring 3.
 */

#include "aster_api.h"

void init(void) {
    aster_api_print_line("Sysapps jsou spoustene v ring 3.");
}
