/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: read – Vypíše obsah souboru.
 * Použití: read <filename>
 */

#include "aster_api.h"

void init(void) {
    int argc = aster_api_get_argc();
    char full[64];
    char buf[4096];
    const char *arg;

    if (argc < 2) {
        aster_api_print_line("Pouziti: read <filename>");
        return;
    }

    arg = aster_api_get_argv(1);
    if (!arg) return;

    aster_api_resolve_path(arg, full, sizeof(full));

    {
        int n = aster_api_file_read_text(full, buf, 4096);
        if (n < 0) {
            aster_api_set_color(0x0C, 0x00);
            aster_api_print_line("Soubor nenalezen");
            aster_api_set_color(0x0F, 0x00);
        } else {
            buf[n] = '\0';
            aster_api_print_line(buf);
        }
    }
}
