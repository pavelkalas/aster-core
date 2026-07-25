/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: copfile – Zkopíruje soubor.
 * Použití: copfile <src> <dst>
 */

#include "aster_api.h"
#include "string.h"

void init(void) {
    int argc = aster_api_get_argc();
    char src[64], dst[64];

    if (argc < 3) {
        aster_api_print_line("Pouziti: copfile <src> <dst>");
        return;
    }

    aster_api_resolve_path(aster_api_get_argv(1), src, sizeof(src));
    aster_api_resolve_path(aster_api_get_argv(2), dst, sizeof(dst));

    /* pokud dst je adresář, přidej basename src */
    if (aster_api_dir_exists(dst)) {
        const char *base = aster_api_path_basename(src);
        usize dlen = aster_strlen(dst);
        usize blen = aster_strlen(base);
        if (dlen + 1 + blen < sizeof(dst)) {
            dst[dlen] = '/';
            {
                usize k;
                for (k = 0; k < blen; ++k) dst[dlen + 1 + k] = base[k];
            }
            dst[dlen + 1 + blen] = '\0';
        } else {
            aster_api_set_color(0x0C, 0x00);
            aster_api_print_line("Cesta prilis dlouha");
            aster_api_set_color(0x0F, 0x00);
            return;
        }
    }

    if (aster_api_copy_file(src, dst) != 0) {
        aster_api_set_color(0x0C, 0x00);
        aster_api_print_line("Chyba kopie");
        aster_api_set_color(0x0F, 0x00);
        return;
    }
    aster_api_print_line("OK");
}
