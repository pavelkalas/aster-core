/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: cd – Změní aktuální adresář.
 * Použití: cd <slozka>
 */

#include "aster_api.h"

void init(void) {
    int argc = aster_api_get_argc();
    char full[64];
    const char *arg;

    if (argc < 2) {
        aster_api_print_line("Pouziti: cd <filename>");
        return;
    }

    arg = aster_api_get_argv(1);
    if (!arg) return;

    aster_api_resolve_path(arg, full, sizeof(full));

    if (aster_api_dir_exists(full)) {
        aster_api_set_cwd(full);
    } else {
        aster_api_set_color(0x0C, 0x00);
        aster_api_print_line("Slozka neexistuje");
        aster_api_set_color(0x0F, 0x00);
    }
}
