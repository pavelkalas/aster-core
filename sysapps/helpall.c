/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: helpall – Interaktivní nápověda – Enter pro další, Q pro konec.
 * Použití: helpall
 */

#include "aster_api.h"
#include "display.h"
#include "drivers.h"

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
    unsigned int i;
    unsigned int total = sizeof(g_help_lines) / sizeof(g_help_lines[0]);

    aster_api_print_line("helpall: Enter = dalsi radek, Q = konec");
    for (i = 0; i < total; ++i) {
        aster_api_print("  ");
        aster_api_print_line(g_help_lines[i]);
        for (;;) {
            int k = keyboard_read_key();
            if (k == '\n') break;
            if (k == 'q' || k == 'Q' || k == 27) {
                aster_api_print_line("helpall ukoncen");
                return;
            }
        }
    }
}
