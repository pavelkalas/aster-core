/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: makdir – Vytvoří nový adresář.
 * Použití: makdir <filename>
 */

#include "aster_api.h"

void init(void) {
    int argc = aster_api_get_argc();
    char full[64];
    const char *arg;

    if (argc < 2) {
        aster_api_print_line("Pouziti: makdir <filename>");
        return;
    }

    arg = aster_api_get_argv(1);
    if (!arg) return;

    aster_api_resolve_path(arg, full, sizeof(full));
    aster_api_print_line(aster_api_dir_create(full) == 0 ? "OK" : "ERROR");
}
