/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: write – Zapíše text do souboru.
 * Použití: write <filename> <text>
 */

#include "aster_api.h"

void init(void) {
    int argc = aster_api_get_argc();
    char full[64];
    const char *fname;
    const char *text;

    if (argc < 3) {
        aster_api_print_line("Pouziti: write <filename> <text>");
        return;
    }

    fname = aster_api_get_argv(1);
    text = aster_api_get_argv(2);
    if (!fname || !text) return;

    aster_api_resolve_path(fname, full, sizeof(full));

    {
        int w = aster_api_file_write_text(full, text);
        if (w < 0) {
            aster_api_set_color(0x0C, 0x00);
            aster_api_print_line("Chyba zapis");
            aster_api_set_color(0x0F, 0x00);
        } else {
            char buf[32];
            int pos = 0;
            const char *pre = "Zapsano ";
            while (*pre) buf[pos++] = *pre++;
            {
                char tmp[16]; int tp = 0; int n = w;
                if (n == 0) tmp[tp++] = '0';
                else { while (n > 0) { tmp[tp++] = (char)('0' + (n % 10)); n /= 10; } }
                while (tp > 0) buf[pos++] = tmp[--tp];
            }
            buf[pos++] = ' ';
            buf[pos++] = 'B';
            buf[pos++] = '\n';
            buf[pos] = '\0';
            aster_api_print(buf);
        }
    }
}
