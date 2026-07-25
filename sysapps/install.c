/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: install – Instalace systému na disk.
 * Použití: install
 */

#include "aster_api.h"
#include "display.h"
#include "drivers.h"

void init(void) {
    static const char motd[] = "Welcome to aster-core v0.13\nType 'help' for commands.\n";
    static const char profile[] = "echo Welcome in aster-core\n";
    static const char installed[] = "installed=1\n";
    char setup_user[32], setup_pass[32], user_home[64];
    int k;

    if (aster_api_system_is_installed()) {
        aster_api_set_color(0x0C, 0x00);
        aster_api_print_line("Neznamy prikaz. Zadej help.");
        aster_api_set_color(0x0F, 0x00);
        return;
    }

    aster_api_print_line("Setup: instalace systemu na AsterFS disk...");

    for (;;) {
        aster_api_print("Setup uzivatel: ");
        aster_api_readline_plain(setup_user, 32);
        if (setup_user[0] == '\0') {
            aster_api_set_color(0x0C, 0x00);
            aster_api_print_line("Uzivatel nesmi byt prazdny");
            aster_api_set_color(0x0F, 0x00);
            continue;
        }
        break;
    }

    aster_api_print("Setup heslo (prazdne = auto-login): ");
    aster_api_readline_secret(setup_pass, 32);
    display_putc('\n');

    if (aster_api_fs_ensure_dir("/etc") != 0 || aster_api_fs_ensure_dir("/home") != 0) {
        aster_api_set_color(0x0C, 0x00);
        aster_api_print_line("Setup error: nelze pripravit adresarovou strukturu");
        aster_api_set_color(0x0F, 0x00);
        return;
    }

    if (setup_user[0]) {
        /* build /home/<user> */
        {
            int p = 0;
            const char *pre = "/home/";
            while (*pre && p < 63) user_home[p++] = *pre++;
            {
                const char *u = setup_user;
                while (*u && p < 63) user_home[p++] = *u++;
            }
            user_home[p] = '\0';
        }
        if (aster_api_fs_ensure_dir(user_home) != 0) {
            aster_api_set_color(0x0C, 0x00);
            aster_api_print("Setup error: home ");
            aster_api_print_line(user_home);
            aster_api_set_color(0x0F, 0x00);
            return;
        }
    }

    if (aster_api_fs_ensure_file_text("/etc/motd", motd) != 0 ||
        aster_api_fs_ensure_file_text("/etc/profile", profile) != 0 ||
        aster_api_fs_ensure_file_text("/.installed", installed) != 0) {
        aster_api_set_color(0x0C, 0x00);
        aster_api_print_line("Setup error: nelze zapsat systemove soubory");
        aster_api_set_color(0x0F, 0x00);
        return;
    }

    /* clear users and add new one */
    if (aster_api_user_add(setup_user, setup_pass) != 0 || aster_api_user_save() != 0) {
        aster_api_set_color(0x0C, 0x00);
        aster_api_print_line("Setup error: nelze ulozit uzivatele");
        aster_api_set_color(0x0F, 0x00);
        return;
    }

    aster_api_print_line("Setup complete: system nainstalovan na disk");
    aster_api_print("Restartovat system nyni? [y/N] ");
    k = keyboard_read_key();
    if (k != '\n') display_putc((char)k);
    display_putc('\n');
    if (k == 'y' || k == 'Y') {
        /* sync before reboot */
        aster_api_reboot();
    }
}
