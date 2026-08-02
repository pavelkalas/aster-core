/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 * Hlavní orchestrator — boot, login, shell smyčka.
 */

#include "auth.h"
#include "boot.h"
#include "bootlog.h"
#include "display.h"
#include "fs_utils.h"
#include "keyboard.h"
#include "printk.h"
#include "shell.h"
#include "statusbar.h"
#include "storage.h"
#include "string.h"
#include "timer.h"
#include "types.h"

extern void boot_sequence(void);
extern char g_cwd[64];

#define SCREEN_W 80U
#define SCREEN_H 25U

static void show_centered_aster_logo(void) {
    static const char logo[][96] = {
        "\xDA\xC4\xBF\xDA\xC4\xBF\xC4\xC2\xC4\xDA\xC4\xC4\xDA\xC4\xBF",
        "\xC3\xC4\xB4\xC0\xC4\xBF \xB3 \xC3\xC4 \xC3\xC2\xD9",
        "\xB3 \xB3\xC0\xC4\xD9 \xB3 \xC0\xC4\xC4\xB3\xC0\xC4"
    };
    const usize logo_lines = sizeof(logo) / sizeof(logo[0]);
    usize max_len = 0;
    usize i;
    usize start_row;

    display_clear();
    display_set_color(0x0B, 0x00);

    for (i = 0; i < logo_lines; ++i) {
        usize len = aster_strlen(logo[i]);
        if (len > max_len) {
            max_len = len;
        }
    }

    start_row = (SCREEN_H > logo_lines) ? (SCREEN_H - logo_lines) / 2U : 0U;

    for (i = 0; i < logo_lines; ++i) {
        usize col = (SCREEN_W > max_len) ? (SCREEN_W - max_len) / 2U : 0U;
        display_set_cursor(start_row + i, col);
        aster_print(logo[i]);
    }

    display_set_color(0x0F, 0x00);
}

/**
 * Hlavní vstupní bod jádra (volaný z boot assembleru).
 * Spustí boot sekvenci, pak v nekonečné smyčce:
 * přihlašovací obrazovka → shell → odhlášení → login.
 */
void kmain(void) {
    int login_enabled;

    boot_sequence();

    show_centered_aster_logo();
    timer_sleep_ms(3000);

    /* Pojistka: pokud se .data sekce nenačetla správně, nastav root */
    if (g_cwd[0] == '\0') {
        g_cwd[0] = '/';
        g_cwd[1] = '\0';
    }

    for (;;) {
        login_enabled = system_is_installed() && auth_has_any_user();
        if (login_enabled) {
            auth_login_screen();
        } else if (g_current_user[0] == '\0') {
            aster_memset(g_current_user, 0, AUTH_NAME_LEN);
            aster_memcpy(g_current_user, "guest", 5);
        }

        display_clear();

        if (system_is_installed()) {
            show_aster_banner("Shell");
        } else {
            display_set_color(0x07, 0x00);
            aster_print("[aster-core v0.13] Copyright (c) 2026 Pavel Kalas\n\n");
            display_set_color(0x04, 0x00);
            aster_print("Tento system neni nainstalovan. Pro instalaci spustte prikaz: install\n");
            display_set_color(0x0F, 0);
            aster_print("\n");
        }
        aster_print("Pro zobrazeni prikazu pouzij 'help [stranka]' nebo 'helpall'\n\n");
        shell_loop();
    }
}
