/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor drzi nouzovou cestu pro neobnovitelne chyby jadra.
 * Pri volani panic prepne vizualni styl vystupu, vypise duvod selhani
 * a bezpecne zastavi CPU, aby se predeslo dalsimu poskozeni stavu.
 */

#include "cpu.h"
#include "display.h"
#include "panic.h"
#include "printk.h"

void panic(const char *reason) {
    display_set_color(0x0F, 0x04);
    printk("\n[KERNEL PANIC] %s\n", reason ? reason : "unknown");
    cpu_halt();
}
