/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: help – Zobrazuje nápovědu po stránkách.
 * Použití: help [stranka]
 */

#include "aster_api.h"

static const char g_help_lines[][80] = {
    "help [stranka]        - napoveda po strankach",
    "helpall               - interaktivni help Enter/Q",
    "info                  - informace o jadru",
    "memory                - stav pameti",
    "process               - seznam procesu",
    "clear                 - vycistit obrazovku",
    "ls                    - vypis aktualni slozky",
    "cd filename           - zmena slozky",
    "makdir filename       - vytvorit slozku",
    "remdir filename       - smazat slozku",
    "movdir src dst [-rec] - presun slozky",
    "copdir src dst [-rec] - kopie slozky",
    "makfile filename      - vytvorit soubor",
    "remfile filename      - smazat soubor",
    "movfile src dst       - presun souboru",
    "copfile src dst       - kopie souboru",
    "read filename         - obsah souboru",
    "write filename text   - zapis textu",
    "install               - instalace systemu na disk",
    "fm                    - file manager",
    "edit filename         - editor (Ctrl+S, ESC)",
    "<sysapps nazvy>       - appka ze slozky sysapps",
    "echo text             - vypis textu",
    "ticks                 - pocet tiknuti casovace",
    "alloc N               - alokovat N bajtu",
    "useradd user [pass]   - vytvori uzivatele",
    "passwdch [user]       - zmeni heslo uzivatele",
    "exit                  - odhlaseni do loginu",
    "reboot                - restart systemu",
    "shutdown              - zastavit system",
};

void init(void) {
    int argc = aster_api_get_argc();
    unsigned int total = sizeof(g_help_lines) / sizeof(g_help_lines[0]);
    unsigned int per_page = 11;
    unsigned int page = 1;
    unsigned int start, end, i;

    if (argc > 1) {
        const char *s = aster_api_get_argv(1);
        unsigned int p = 0;
        if (s) {
            while (*s >= '0' && *s <= '9') { p = p * 10 + (unsigned int)(*s - '0'); ++s; }
        }
        if (p < 1 || p > 3) {
            aster_api_print_line("Pouziti: help [1..3]");
            return;
        }
        page = p;
    }

    start = (page - 1) * per_page;
    end = start + per_page;
    if (end > total) end = total;

    {
        aster_api_print("Help stranka ");
        /* simple itoa-like */
        {
            char tmp[12]; int pos = 0; unsigned int n = page;
            if (n == 0) tmp[pos++] = '0';
            else { while (n > 0) { tmp[pos++] = (char)('0' + (n % 10)); n /= 10; } }
            while (pos > 0) { char c = tmp[--pos]; aster_api_print((char[]){c, '\0'}); }
        }
        aster_api_print("/");
        {
            char tmp[12]; int pos = 0; unsigned int n = (total + per_page - 1) / per_page;
            if (n == 0) tmp[pos++] = '0';
            else { while (n > 0) { tmp[pos++] = (char)('0' + (n % 10)); n /= 10; } }
            while (pos > 0) { char c = tmp[--pos]; aster_api_print((char[]){c, '\0'}); }
        }
        aster_api_print_line("");

    }

    for (i = start; i < end; ++i) {
        aster_api_print("  ");
        aster_api_print_line(g_help_lines[i]);
    }
}
