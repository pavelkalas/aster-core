/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Tento modul sjednocuje stavove vypisy jadra ve formatu [ STAV ] zprava.
 * Poskytuje jednoduche helpery pro bezne uspesne a chybove hlaseni,
 * aby byly boot/setup logy konzistentni na jednom miste.
 */

#include "bootlog.h"

#include "display.h"
#include "printk.h"

void bootlog_state(const char *state, unsigned char state_color, const char *msg) {
    display_set_color(0x0F, 0x00);
    aster_print("[ ");
    display_set_color(state_color, 0x00);
    aster_print(state ? state : "NEZNAMY");
    display_set_color(0x0F, 0x00);
    aster_print(" ] ");
    printk("%s\n", msg ? msg : "");
}

void bootlog_ok(const char *msg) {
    bootlog_state("HOTOVO", 0x0A, msg);
}

void bootlog_error(const char *msg) {
    bootlog_state("CHYBA", 0x0C, msg);
}