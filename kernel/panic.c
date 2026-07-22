/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor drží nouzovou cestu pro neobnovitelné chyby jádra.
 * Při volání panic přepne vizuální styl výstupu, vypíše důvod selhání
 * a bezpečně zastaví CPU, aby se předešlo dalšímu poškození stavu.
 */

#include "cpu.h"
#include "display.h"
#include "panic.h"
#include "printk.h"

/**
 * Zastaví jádro při neobnovitelné chybě – vypíše důvod a zastaví CPU.
 *
 * @param reason Textový popis důvodu paniky (const char *)
 */
void panic(const char *reason) {
    display_set_color(0x0F, 0x04);
    printk("\n[KERNEL PANIC] %s\n", reason ? reason : "unknown");
    cpu_halt();
}
