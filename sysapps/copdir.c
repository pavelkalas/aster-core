/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: copdir – Zkopíruje adresář.
 * Použití: copdir <src> <dst> [-rec]
 */

#include "aster_api.h"
#include "string.h"

void init(void) {
    int argc = aster_api_get_argc();
    char src[64], dst[64];
    int rec = 0;

    if (argc < 3) {
        aster_api_print_line("Pouziti: copdir <src> <dst> [-rec]");
        return;
    }

    aster_api_resolve_path(aster_api_get_argv(1), src, sizeof(src));
    aster_api_resolve_path(aster_api_get_argv(2), dst, sizeof(dst));

    if (argc > 3 && aster_strcmp(aster_api_get_argv(3), "-rec") == 0) {
        rec = 1;
    } else if (argc > 3) {
        aster_api_set_color(0x0C, 0x00);
        aster_api_print_line("Neznamy prepinac");
        aster_api_set_color(0x0F, 0x00);
        return;
    }

    {
        int rc = aster_api_copy_dir(src, dst, rec);
        if (rc == -2) {
            aster_api_set_color(0x0C, 0x00);
            aster_api_print_line("Slozka neni prazdna. Pouzij -rec");
            aster_api_set_color(0x0F, 0x00);
            return;
        }
        if (rc != 0) {
            aster_api_set_color(0x0C, 0x00);
            aster_api_print_line("Chyba kopie");
            aster_api_set_color(0x0F, 0x00);
            return;
        }
        aster_api_print_line("OK");
    }
}
