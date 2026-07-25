/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: remdir – Smaže adresář.
 * Použití: remdir <filename>
 */

#include "aster_api.h"
#include "string.h"

void init(void) {
    int argc = aster_api_get_argc();
    char full[64];
    const char *arg;

    if (argc < 2) {
        aster_api_print_line("Pouziti: remdir <filename>");
        return;
    }

    arg = aster_api_get_argv(1);
    if (!arg) return;

    aster_api_resolve_path(arg, full, sizeof(full));

    if (aster_strcmp(full, "/") == 0) {
        aster_api_set_color(0x0C, 0x00);
        aster_api_print_line("Nelze smazat root /");
        aster_api_set_color(0x0F, 0x00);
        return;
    }

    if (!aster_api_dir_exists(full)) {
        aster_api_set_color(0x0C, 0x00);
        aster_api_print_line("Neexistuje");
        aster_api_set_color(0x0F, 0x00);
        return;
    }

    if (aster_api_dir_remove(full) == 0 || aster_api_remove_tree(full) == 0) {
        aster_api_print_line("OK");
    } else {
        aster_api_set_color(0x0C, 0x00);
        aster_api_print_line("Chyba");
        aster_api_set_color(0x0F, 0x00);
    }
}
