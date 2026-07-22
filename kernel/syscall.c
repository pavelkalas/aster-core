/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento modul je vstupní brána pro systémová volání.
 * Obsahuje dispatcher, který podle ID volá konkrétní kernelovou službu,
 * a implementuje základní syscall operace pro zápis, alokaci a procesy.
 */

#include "display.h"
#include "drivers.h"
#include "memory.h"
#include "printk.h"
#include "process.h"
#include "scheduler.h"
#include "storage.h"
#include "timer.h"
#include "syscall.h"

/**
 * Inicializuje vrstvu syscallů (v této implementaci prázdná).
 */
void syscall_init(void) {
}

/**
 * Hlavní dispatcher systémových volání.
 * Podle ID zavolá příslušnou kernelovou funkci.
 *
 * @param id Identifikátor syscallu (u64)
 * @param a1 První argument (u64)
 * @param a2 Druhý argument (u64)
 * @param a3 Třetí argument (u64, nepoužitý)
 * @param a4 Čtvrtý argument (u64, nepoužitý)
 * @return   Návratová hodnota volané funkce (u64)
 */
u64 syscall_dispatch(u64 id, u64 a1, u64 a2, u64 a3, u64 a4) {
    (void)a3;
    (void)a4;

    switch (id) {
        case SYSCALL_WRITE:
            return aster_write((const char *)a1);
        case SYSCALL_ALLOC:
            return (u64)aster_alloc((usize)a1);
        case SYSCALL_PROCESS_CREATE:
            return (u64)aster_process_create((void (*)(void))a1, (const char *)a2, (u8)a3);
        case SYSCALL_CLEAR:
            return aster_clear();
        case SYSCALL_TICKS:
            return aster_ticks();
        case SYSCALL_SLEEP_MS:
            return aster_sleep_ms(a1);
        case SYSCALL_READ_KEY:
            return (u64)aster_read_key();
        case SYSCALL_TRY_READ_KEY:
            return (u64)aster_try_read_key();
        case SYSCALL_READ_LINE:
            return (u64)aster_read_line((char *)a1, (i64)a2);
        case SYSCALL_FS_CREATE_FILE:
            return (u64)asterfs_create_file_sys((const char *)a1);
        case SYSCALL_FS_READ_FILE:
            return (u64)asterfs_read_file_sys((const char *)a1, (u8 *)a2, (u16)a3);
        case SYSCALL_FS_WRITE_FILE:
            return (u64)asterfs_write_file_sys((const char *)a1, (const u8 *)a2, (u16)a3);
        case SYSCALL_FS_CREATE_DIR:
            return (u64)asterfs_create_dir_sys((const char *)a1);
        case SYSCALL_FS_REMOVE_FILE:
            return (u64)asterfs_remove_file_sys((const char *)a1);
        case SYSCALL_FS_REMOVE_DIR:
            return (u64)asterfs_remove_dir_sys((const char *)a1);
        case SYSCALL_PROCESS_COUNT:
            return (u64)aster_process_count_sys();
        case SYSCALL_PROCESS_CURPID:
            return (u64)aster_process_current_pid();
        case SYSCALL_YIELD:
            return aster_yield();
        default:
            return (u64)-1;
    }
}

/**
 * Vypíše text na obrazovku (wrapper pro printk).
 *
 * @param text Řetězec k vypsání (const char *)
 * @return     0 při úspěchu, -1 při chybě (u64)
 */
u64 aster_write(const char *text) {
    if (!text) {
        return (u64)-1;
    }

    printk("%s", text);
    return 0;
}

/**
 * Alokuje paměť na kernelové haldě.
 *
 * @param size Velikost v bajtech (usize)
 * @return     Ukazatel na alokovanou paměť, nebo NULL (void *)
 */
void *aster_alloc(usize size) {
    return kmalloc(size);
}

/**
 * Vytvoří nový proces.
 *
 * @param entry    Vstupní bod (void (*)(void))
 * @param name     Název (const char *)
 * @param priority Priorita (u8)
 * @return         PID nového procesu, nebo -1 při chybě (i64)
 */
i64 aster_process_create(void (*entry)(void), const char *name, u8 priority) {
    process_t *p = process_create(name, entry, priority);
    if (!p) {
        return -1;
    }

    return p->pid;
}

/**
 * Smaže obrazovku.
 *
 * @return 0 (u64)
 */
u64 aster_clear(void) {
    display_clear();
    return 0;
}

/**
 * Vrátí počet tiků časovače od startu.
 *
 * @return Počet tiků (u64)
 */
u64 aster_ticks(void) {
    return (u64)timer_ticks();
}

/**
 * Zpozdí běh o zadaný počet milisekund.
 *
 * @param ms Počet milisekund (u64)
 * @return   0 (u64)
 */
u64 aster_sleep_ms(u64 ms) {
    timer_sleep_ms((unsigned long)ms);
    return 0;
}

/**
 * Přečte jednu klávesu (blokující).
 *
 * @return Kód klávesy (i64)
 */
i64 aster_read_key(void) {
    return (i64)keyboard_read_key();
}

/**
 * Přečte klávesu, pokud je k dispozici (neblokující).
 *
 * @return Kód klávesy, nebo -1 (i64)
 */
i64 aster_try_read_key(void) {
    return (i64)keyboard_try_read_key();
}

/**
 * Přečte řádek textu z klávesnice.
 *
 * @param buffer  Cílový buffer (char *)
 * @param max_len Maximální délka (i64)
 * @return        Počet přečtených znaků, nebo -1 (i64)
 */
i64 aster_read_line(char *buffer, i64 max_len) {
    if (!buffer || max_len <= 0) {
        return -1;
    }

    return (i64)keyboard_readline(buffer, (int)max_len);
}

/**
 * Vytvoří soubor (varianta pro syscall – vrací i64).
 *
 * @param path Cesta (const char *)
 * @return     0 při úspěchu, -1 při chybě (i64)
 */
i64 asterfs_create_file_sys(const char *path) {
    return (i64)asterfs_create_file(path);
}

/**
 * Přečte soubor (varianta pro syscall – vrací i64).
 *
 * @param path    Cesta (const char *)
 * @param out     Výstupní buffer (u8 *)
 * @param max_len Maximální délka (u16)
 * @return        Počet přečtených bajtů, nebo -1 (i64)
 */
i64 asterfs_read_file_sys(const char *path, u8 *out, u16 max_len) {
    return (i64)asterfs_read_file(path, out, max_len);
}

/**
 * Zapíše do souboru (varianta pro syscall – vrací i64).
 *
 * @param path Cesta (const char *)
 * @param data Data k zápisu (const u8 *)
 * @param len  Délka dat (u16)
 * @return     Počet zapsaných bajtů, nebo -1 (i64)
 */
i64 asterfs_write_file_sys(const char *path, const u8 *data, u16 len) {
    return (i64)asterfs_write_file(path, data, len);
}

/**
 * Vytvoří adresář (varianta pro syscall – vrací i64).
 *
 * @param path Cesta (const char *)
 * @return     0 při úspěchu, -1 při chybě (i64)
 */
i64 asterfs_create_dir_sys(const char *path) {
    return (i64)asterfs_create_dir(path);
}

/**
 * Smaže soubor (varianta pro syscall – vrací i64).
 *
 * @param path Cesta (const char *)
 * @return     0 při úspěchu, -1 při chybě (i64)
 */
i64 asterfs_remove_file_sys(const char *path) {
    return (i64)asterfs_remove_file(path);
}

/**
 * Smaže adresář (varianta pro syscall – vrací i64).
 *
 * @param path Cesta (const char *)
 * @return     0 při úspěchu, -1 při chybě (i64)
 */
i64 asterfs_remove_dir_sys(const char *path) {
    return (i64)asterfs_remove_dir(path);
}

/**
 * Vrátí počet procesů v tabulce (varianta pro syscall).
 *
 * @return Počet procesů (i64)
 */
i64 aster_process_count_sys(void) {
    return (i64)process_count();
}

/**
 * Vrátí PID aktuálního procesu.
 *
 * @return PID, nebo -1 pokud není aktuální proces (i64)
 */
i64 aster_process_current_pid(void) {
    process_t *p = process_current();

    if (!p) {
        return -1;
    }

    return (i64)p->pid;
}

/**
 * Předá řízení plánovači (yield).
 *
 * @return 0 (u64)
 */
u64 aster_yield(void) {
    scheduler_yield();
    return 0;
}
