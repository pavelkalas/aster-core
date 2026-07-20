/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 * Hlavni orchestrator — boot, login, shell smycka.
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

void kmain(void) {
    boot_sequence();

    for (;;) {
        auth_login_screen();
        ensure_aliases_file();
        display_clear();

        if (system_is_installed()) {
            show_aster_banner("Shell");
        } else {
            display_set_color(0x07, 0x00);
            aster_print("[aster-core v0.13] Copyright (c) 2026 Pavel Kalas\n\n");
            display_set_color(0x0E, 0x00);
            aster_print("Pro instalaci na disk spust prikaz: install\n");
            display_set_color(0x0F, 0);
        }
        aster_print("Pro zobrazeni prikazu pouzij 'help'\n\n");
        shell_loop();
    }
}
