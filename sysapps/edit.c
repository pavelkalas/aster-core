/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: edit – Spustí textový editor pro daný soubor.
 * Použití: edit <filename>
 */

#include "aster_api.h"

void init(void) {
    int argc = aster_api_get_argc();
    char full[64];

    if (argc < 2) {
        aster_api_print_line("Pouziti: edit <filename>");
        return;
    }

    aster_api_resolve_path(aster_api_get_argv(1), full, sizeof(full));
    aster_api_edit_file(full);
    aster_api_render_statusbar();
}
