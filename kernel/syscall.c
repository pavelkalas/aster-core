/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento modul je vstupni brana pro systemova volani.
 * Obsahuje dispatcher, ktery podle ID vola konkretni kernelovou sluzbu,
 * a implementuje zakladni syscall operace pro zapis, alokaci a procesy.
 */

#include "memory.h"
#include "printk.h"
#include "process.h"
#include "syscall.h"

void syscall_init(void) {
}

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
        default:
            return (u64)-1;
    }
}

u64 aster_write(const char *text) {
    if (!text) {
        return (u64)-1;
    }

    printk("%s", text);
    return 0;
}

void *aster_alloc(usize size) {
    return kmalloc(size);
}

i64 aster_process_create(void (*entry)(void), const char *name, u8 priority) {
    process_t *p = process_create(name, entry, priority);
    if (!p) {
        return -1;
    }

    return p->pid;
}
