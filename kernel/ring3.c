/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Ring 3 – funkce pro vstup do uživatelského módu (ring 3)
 * a syscall handler pro obsluhu SYSCALL instrukce.
 */

#include "cpu.h"
#include "memory.h"
#include "printk.h"
#include "syscall.h"
#include "types.h"

/* Externí symboly z assembleru */
extern void syscall_init(void);
extern u64 g_kernel_syscall_stack_top;

/** Stack pro ring 0 při přerušení z ring 3. */
static u8 g_ring0_stack[4096] __attribute__((aligned(16)));

/**
 * Inicializace ring 3 – nastaví TSS.RSP0 a zapíše syscall MSR.
 * Volá se při bootu po cpu_init().
 */
void ring3_init(void) {
    /* Nastavit kernel stack pro přechody ring 3 -> ring 0 */
    u64 rsp0 = (u64)&g_ring0_stack[sizeof(g_ring0_stack) - 16];
    tss_set_rsp0(rsp0);

    /* Nastavit stack pro syscall handler */
    g_kernel_syscall_stack_top = (u64)&g_ring0_stack[sizeof(g_ring0_stack) - 16];

    /* Inicializovat SYSCALL MSR */
    syscall_init();
}

/**
 * Vstoupí do ring 3 (uživatelský mód) na zadané adrese.
 * Nastaví stack a pomocí IRETQ přepne CPU do ring 3.
 *
 * @param entry_point Adresa vstupního bodu (ring 3) (u64)
 * @param user_stack  Vrchol uživatelského zásobníku (u64)
 */
void enter_ring3(u64 entry_point, u64 user_stack) {
    /*
     * Zmapovat stránky pro ring 3 jako user-accessible.
     * entry_point by měl být v adresním prostoru,
     * který je mapován s USER bitem.
     */

    /* Nastavit segmentové selektory pro ring 3 */
    __asm__ volatile (
        "cli\n"
        "mov %[user_ds], %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"

        /* Připravit stack frame pro IRETQ */
        "push %[user_ss]\n"           /* SS */
        "push %[user_rsp]\n"          /* RSP */
        "pushf\n"                     /* RFLAGS */
        "push %[user_cs]\n"           /* CS */
        "push %[user_rip]\n"          /* RIP */

        "iretq\n"
        :
        : [user_ss]   "i"(GDT_USER_DATA),
          [user_rsp]  "r"(user_stack),
          [user_cs]   "i"(GDT_USER_CODE),
          [user_rip]  "r"(entry_point),
          [user_ds]   "i"(GDT_USER_DATA)
        : "ax", "memory"
    );
}

/**
 * Syscall handler – volán z assembly syscall_entry.
 * Dispečuje systémová volání z ring 3.
 *
 * @param number Číslo syscallu (u64)
 * @param a1     První argument (u64)
 * @param a2     Druhý argument (u64)
 * @param a3     Třetí argument (u64)
 * @return       Návratová hodnota syscallu (u64)
 */
u64 syscall_handler(u64 number, u64 a1, u64 a2, u64 a3) {
    switch (number) {
        case 0:  /* write – výpis textu */
            if (a1) {
                printk("%s", (const char *)a1);
            }
            return 0;

        case 1:  /* exit – ukončení procesu */
            printk("\n[ring3] proces ukoncen, navrat do kernelu\n");
            /* Zde by měl scheduler přepnout na jiný proces,
             * prozatím zůstaneme v nekonečné smyčce. */
            for (;;) {
                __asm__ volatile ("hlt");
            }

        case 2:  /* clear – vyčištění obrazovky */
            return (u64)syscall_dispatch(4 /* SYSCALL_CLEAR */, 0, 0, 0, 0);

        default:
            return (u64)-1;
    }
}
