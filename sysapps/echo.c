/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: echo – Vypíše text na obrazovku.
 * Použití: echo <text>
 */

#include "aster_api.h"

void init(void) {
    int argc = aster_api_get_argc();
    int i;
    for (i = 1; i < argc; ++i) {
        if (i > 1) aster_api_print(" ");
        aster_api_print(aster_api_get_argv(i));
    }
    aster_api_print_line("");
}
