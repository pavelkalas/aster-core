/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: useradd – Vytvoří nového uživatele.
 * Použití: useradd <user> [pass]
 */

#include "aster_api.h"
#include "string.h"

void init(void) {
    int argc = aster_api_get_argc();
    const char *uname;
    const char *pass;
    char pass_buf[32];

    if (argc < 2) {
        aster_api_print_line("Pouziti: useradd <user> [pass]");
        return;
    }

    uname = aster_api_get_argv(1);
    if (!uname) return;

    pass = aster_api_get_argv(2);
    pass_buf[0] = '\0';

    if (pass) {
        usize plen = aster_strlen(pass);
        if (plen >= 32) {
            aster_api_set_color(0x0C, 0x00);
            aster_api_print_line("Heslo moc dlouhe");
            aster_api_set_color(0x0F, 0x00);
            return;
        }
        {
            usize k;
            for (k = 0; k < plen; ++k) pass_buf[k] = pass[k];
        }
        pass_buf[plen] = '\0';
    }

    if (aster_api_user_add(uname, pass_buf) != 0 || aster_api_user_save() != 0) {
        aster_api_set_color(0x0C, 0x00);
        aster_api_print_line("Chyba");
        aster_api_set_color(0x0F, 0x00);
    } else {
        aster_api_print_line("OK");
    }
}
