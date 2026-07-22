/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 * Boot — inicializační sekvence jádra, volaná z kmain.
 * Postupně inicializuje všechny subsystémy: display, sériový port, CPU,
 * správce paměti, procesy, plánovač, syscally, klávesnici, časovač,
 * disk a přerušení.
 */

#include "boot.h"
#include "bootlog.h"
#include "cpu.h"
#include "display.h"
#include "drivers.h"
#include "int.h"
#include "memory.h"
#include "printk.h"
#include "process.h"
#include "scheduler.h"
#include "serial.h"
#include "statusbar.h"
#include "storage.h"
#include "syscall.h"
#include "timer.h"

extern unsigned long g_timer_hz;

/**
 * Zpětné volání pro obnovení stavového řádku shellu.
 * Voláno periodicky z klávesnicového ovladače.
 */
void shell_status_refresh_callback(void) {
    render_shell_statusbar();
}

/**
 * Hlavní bootovací sekvence – postupně inicializuje všechny subsystémy:
 * display, sériový port, CPU, správce paměti, procesy, plánovač,
 * syscally, klávesnici, časovač, disk a přerušení.
 * Každý krok je zalogován pomocí bootlogu.
 */
void boot_sequence(void) {
    display_init();

    boot_step_begin("Serial ovladac");
    serial_init();
    if (serial_is_ready()) boot_step_ok("Serial ovladac");
    else boot_step_skip("Serial ovladac");

    boot_step_begin("CPU jadro");
    cpu_init();
    boot_step_ok("CPU jadro");

    boot_step_begin("Spravce pameti nacten");
    memory_init();
    boot_step_ok("Spravce pameti nacten");

    boot_step_begin("Spravce procesu");
    process_init();
    boot_step_ok("Spravce procesu");

    boot_step_begin("Planovac");
    scheduler_init();
    boot_step_ok("Planovac");

    boot_step_begin("Vrstva syscalls");
    syscall_init();
    boot_step_ok("Vrstva syscalls");

    boot_step_begin("Klavesnicovy ovladac");
    keyboard_init();
    boot_step_ok("Klavesnicovy ovladac");

    boot_step_begin("Casovac");
    timer_init((unsigned int)g_timer_hz);
    boot_step_ok("Casovac");

    boot_step_begin("Obnova shellu");
    keyboard_set_refresh_callback(shell_status_refresh_callback, g_timer_hz ? g_timer_hz : 100UL);
    boot_step_ok("Obnova shellu");

    boot_step_begin("Diskovy ovladac");
    storage_init();
    boot_step_ok("Diskovy ovladac");

    boot_step_begin("Preruseni");
    interrupts_init();
    boot_step_ok("Preruseni");

    aster_print("\nBoot sekvence dokoncena\n");
    timer_sleep_ms(1000);
}

