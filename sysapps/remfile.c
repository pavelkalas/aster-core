/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: remfile – Smaže soubor.
 * Použití: remfile <filename>
 */

#include "aster_api.h"

void init(void) {
    int argc = aster_api_get_argc();
    char full[64];
    const char *arg;

    if (argc < 2) {
        aster_api_print_line("Pouziti: remfile <filename>");
        return;
    }

    arg = aster_api_get_argv(1);
    if (!arg) return;

    aster_api_resolve_path(arg, full, sizeof(full));
    aster_api_print_line(aster_api_file_remove(full) == 0 ? "OK" : "ERROR");
}
