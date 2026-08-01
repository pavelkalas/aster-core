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
#include "ring3.h"
#include "string.h"
#include "syscall.h"
#include "types.h"

/* Externí symboly z assembleru */
extern void syscall_init(void);
extern u64 g_kernel_syscall_stack_top;
extern char ring3_sysapp_start;
extern char ring3_sysapp_end;
extern char ring3_sysapp_entry_ptr;

/** Stack pro ring 0 při přerušení z ring 3. */
static u8 g_ring0_stack[4096] __attribute__((aligned(16)));

#define USER_TRAMP_ADDR 0x400000ULL
#define USER_STACK_TOP  (USER_TRAMP_ADDR + 0x200000ULL - 16)

/* Priznaky aktivniho ring3 behu jedne sysapp instance. */
static volatile int g_ring3_active = 0;
static volatile int g_ring3_exit_code = 0;

/* Ulozeny kernel kontext pro navrat po int 0x81 z ring3 trampoline. */
static u64 g_return_rip = 0;
static u64 g_return_rsp = 0;
static u64 g_return_rbp = 0;
static u64 g_return_rflags = 0;

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
        "pushfq\n"
        "pop %%rax\n"
        "or %[iopl], %%rax\n"
        "push %%rax\n"                /* RFLAGS s IOPL=3 */
        "push %[user_cs]\n"           /* CS */
        "push %[user_rip]\n"          /* RIP */

        "iretq\n"
        :
        : [user_ss]   "i"(GDT_USER_DATA),
          [user_rsp]  "r"(user_stack),
          [user_cs]   "i"(GDT_USER_CODE),
          [user_rip]  "r"(entry_point),
          [user_ds]   "i"(GDT_USER_DATA),
          [iopl]      "i"(0x3000ULL)
        : "rax", "memory"
    );
}

/**
 * Spusti sysapp v ring 3 pres maly trampoline kod.
 * Trampoline po navratu z aplikace vyvola software interrupt,
 * ktery vrati rideni zpatky do kernelu/shellu.
 *
 * @param entry Vstupni funkce sysapp (void (*)(void))
 * @return      0 pri uspechu, jinak -1
 */
int ring3_run_sysapp(void (*entry)(void)) {
    usize tramp_size;
    usize entry_off;
    u8 *dst;

    if (!entry) {
        return -1;
    }

    tramp_size = (usize)(&ring3_sysapp_end - &ring3_sysapp_start);
    entry_off = (usize)(&ring3_sysapp_entry_ptr - &ring3_sysapp_start);
    if (tramp_size == 0 || entry_off + sizeof(u64) > tramp_size) {
        return -1;
    }

    dst = (u8 *)USER_TRAMP_ADDR;
    aster_memcpy(dst, &ring3_sysapp_start, tramp_size);
    *((u64 *)(dst + entry_off)) = (u64)entry;

    /*
     * Sysapps jsou stale zkompilovane jako soucast kernel image,
     * proto docasne povolime user pristup i na nizke mapy jadra.
     */
    page_map_user_2mb(0x000000ULL);
    page_map_user_2mb(0x200000ULL);
    page_map_user_2mb(USER_TRAMP_ADDR);

    __asm__ volatile (
        "mov %%rsp, %0\n"
        "mov %%rbp, %1\n"
        "pushfq\n"
        "pop %2\n"
        : "=m"(g_return_rsp),
          "=m"(g_return_rbp),
          "=m"(g_return_rflags)
        :
        : "memory"
    );

    g_ring3_exit_code = 0;
    g_ring3_active = 1;
    g_return_rip = (u64)&&ring3_resume;
    enter_ring3(USER_TRAMP_ADDR, USER_STACK_TOP);

ring3_resume:
    g_ring3_active = 0;
    return (int)g_ring3_exit_code;
}

/**
 * Obsluha software interruptu pro navrat z ring 3.
 * Pokud jsme uvnitr ring3 behu sysapp, prepise interrupt frame tak,
 * aby iretq pokracoval zpet na ulozenem kernel navratu.
 *
 * @return 1 pokud byl navrat zpracovan, jinak 0.
 */
int ring3_handle_return_vector(u64 vector,
                               u64 cs,
                               u64 *cs_out,
                               u64 *rip,
                               u64 *rsp,
                               u64 *rflags,
                               u64 *ss,
                               u64 *rbp) {
    if (vector != (u64)RING3_RETURN_VECTOR) {
        return 0;
    }
    if (!g_ring3_active) {
        return 0;
    }
    if ((cs & 0x3ULL) != 0x3ULL) {
        return 0;
    }
    if (!cs_out || !rip || !rsp || !rflags || !ss || !rbp) {
        return 0;
    }

    *cs_out = GDT_KERNEL_CODE;
    *rip = g_return_rip;
    *rsp = g_return_rsp;
    *rbp = g_return_rbp;
    *rflags = g_return_rflags;
    *ss = GDT_KERNEL_DATA;
    g_ring3_exit_code = 0;
    g_ring3_active = 0;
    return 1;
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
    (void)a2;
    (void)a3;

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
