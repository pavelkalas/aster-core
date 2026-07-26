/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: ring3 – spustí testovací Ring 3 program.
 * Použití: ring3
 *
 * Zkopíruje ring3_test kód do user-accessible paměti,
 * nastaví USER bit v page tabulkách a vstoupí do ring 3.
 */

#include "aster_api.h"
#include "memory.h"
#include "ring3.h"
#include "string.h"

/* Externí symboly z ring3test.asm */
extern char ring3_test_start;
extern char ring3_test_end;

/** Virtuální adresa pro user program (2MB-aligned). */
#define USER_PROG_ADDR 0x400000ULL
#define USER_STACK_TOP (USER_PROG_ADDR + 0x200000ULL - 16)

void init(void) {
    usize code_size;
    char *src;
    char *dst;

    src = &ring3_test_start;
    dst = (char *)USER_PROG_ADDR;
    code_size = (usize)(&ring3_test_end - &ring3_test_start);

    if (code_size == 0 || code_size > 0x100000) {
        aster_api_print_line("Ring 3 test: chyba velikosti kodu");
        return;
    }

    /* Zkopírovat kód do user oblasti */
    aster_memcpy(dst, src, code_size);

    /* Nastavit USER bit na stránkách pro user program */
    page_map_user_2mb(USER_PROG_ADDR);

    aster_api_print_line("Vstupuji do Ring 3...");

    /* Vstup do ring 3 */
    enter_ring3(USER_PROG_ADDR, USER_STACK_TOP);

    /* Po návratu z ring 3 (exit syscall způsobí halt) */
    aster_api_print_line("Navrat z Ring 3");
}
