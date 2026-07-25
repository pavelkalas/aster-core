/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: ls – Vypíše obsah aktuálního adresáře.
 * Použití: ls
 */

#include "aster_api.h"

static void ls_cb(const aster_api_dirent_t *entry, void *user) {
    (void)user;
    aster_api_print(entry->is_dir ? "DIR  " : "FILE ");
    aster_api_print(entry->name);
    aster_api_print("  ");
    /* print size */
    {
        char tmp[16]; int tp = 0; u16 n = entry->size;
        if (n == 0) tmp[tp++] = '0';
        else { while (n > 0) { tmp[tp++] = (char)('0' + (n % 10)); n /= 10; } }
        while (tp > 0) { char c = tmp[--tp]; aster_api_print((char[]){c, '\0'}); }
    }
    aster_api_print_line("");
}

void init(void) {
    char cwd[64];
    aster_api_get_cwd(cwd, sizeof(cwd));
    aster_api_list_dir(cwd, ls_cb, 0);
}
