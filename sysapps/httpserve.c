/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 * Sysapp pro zpřístupnění souboru, adresáře nebo domovské stránky přes HTTP.
 */

#include "aster_api.h"
#include "drivers.h"
#include "network.h"

/**
 * @brief Převede textový zápis TCP portu na 16bitovou hodnotu.
 *
 * @param text Desítkový zápis portu.
 * @param port Výstupní proměnná pro platný port.
 * @return 0 při úspěchu; -1 pro neplatný nebo nulový port.
 */
static int parse_port(const char *text, u16 *port) {
    u32 value = 0;

    if (!text || !*text || !port) {
        return -1;
    }

    while (*text) {
        if (*text < '0' || *text > '9') {
            return -1;
        }
        value = value * 10U + (u32)(*text - '0');
        if (value > 65535U) {
            return -1;
        }
        ++text;
    }

    if (value == 0U) {
        return -1;
    }

    *port = (u16)value;
    return 0;
}

/**
 * @brief Zjistí, zda argument označuje vestavěnou domovskou stránku.
 *
 * @param argument Argument z příkazové řádky.
 * @return 1 pouze pro přesný token `...`; jinak 0.
 */
static int is_default_page_argument(const char *argument) {
    return argument && argument[0] == '.' && argument[1] == '.'
        && argument[2] == '.' && argument[3] == '\0';
}

/**
 * @brief Spustí sysapp `httpserve` a obsluhuje jej do stisku Q.
 *
 * Podporuje `httpserve <soubor|slozka> <port>` i
 * `httpserve ... <port>` pro vestavěnou domovskou stránku.
 */
void init(void) {
    int argc = aster_api_get_argc();
    const char *argument;
    const char *port_text;
    char path[ASTERFS_NAME_LEN];
    u16 port;

    if (argc != 3) {
        aster_api_print_line("Pouziti: httpserve <soubor|slozka> <port>");
        return;
    }

    argument = aster_api_get_argv(1);
    port_text = aster_api_get_argv(2);
    if (!argument || parse_port(port_text, &port) != 0) {
        aster_api_print_line("Port musi byt cislo 1..65535");
        return;
    }

    if (is_default_page_argument(argument)) {
        if (network_http_serve_default(port) != 0) {
            aster_api_set_color(0x0C, 0x00);
            aster_api_print_line("HTTP server nelze spustit");
            aster_api_set_color(0x0F, 0x00);
            return;
        }
        aster_api_print("HTTP server bezi na http://localhost:");
        aster_api_print(port_text);
        aster_api_print_line("\nZdroj: vestavena domovska stranka");
    } else {
        aster_api_resolve_path(argument, path, sizeof(path));
        if (!aster_api_path_exists(path)) {
            aster_api_set_color(0x0C, 0x00);
            aster_api_print_line("Soubor nebo slozka neexistuje");
            aster_api_set_color(0x0F, 0x00);
            return;
        }

        if (network_http_serve(path, port) != 0) {
            aster_api_set_color(0x0C, 0x00);
            aster_api_print_line("HTTP server nelze spustit");
            aster_api_set_color(0x0F, 0x00);
            return;
        }

        aster_api_print("HTTP server bezi na http://localhost:");
        aster_api_print(port_text);
        aster_api_print("\nZdroj: ");
        aster_api_print_line(path);
    }

    aster_api_print_line("Pro zastaveni stiskni Q.");

    for (;;) {
        int key = keyboard_read_key();

        if (key == 'q' || key == 'Q') {
            network_http_stop();
            aster_api_print_line("HTTP server zastaven");
            return;
        }
    }
}
