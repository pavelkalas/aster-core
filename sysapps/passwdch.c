/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: passwdch – Změní heslo uživatele.
 * Použití: passwdch [user]
 */

#include "aster_api.h"

void init(void) {
    int argc = aster_api_get_argc();
    char target[32];
    char new_pass[32];

    if (argc > 1) {
        const char *arg = aster_api_get_argv(1);
        if (arg) {
            usize i = 0;
            while (*arg && i < 31) target[i++] = *arg++;
            target[i] = '\0';
        } else {
            aster_api_get_current_user(target, sizeof(target));
        }
    } else {
        aster_api_get_current_user(target, sizeof(target));
    }

    if (target[0] == '\0' || aster_api_user_find(target) < 0) {
        aster_api_set_color(0x0C, 0x00);
        aster_api_print_line("Neexistuje");
        aster_api_set_color(0x0F, 0x00);
        return;
    }

    aster_api_print("Nove heslo: ");
    aster_api_readline_secret(new_pass, sizeof(new_pass));

    if (aster_api_user_set_pass(target, new_pass) != 0 || aster_api_user_save() != 0) {
        aster_api_set_color(0x0C, 0x00);
        aster_api_print_line("Chyba");
        aster_api_set_color(0x0F, 0x00);
    } else {
        aster_api_print_line("OK");
    }
}
