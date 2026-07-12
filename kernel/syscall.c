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

#include "display.h"
#include "drivers.h"
#include "memory.h"
#include "printk.h"
#include "process.h"
#include "scheduler.h"
#include "storage.h"
#include "timer.h"
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

u64 aster_clear(void) {
    display_clear();
    return 0;
}

u64 aster_ticks(void) {
    return (u64)timer_ticks();
}

u64 aster_sleep_ms(u64 ms) {
    timer_sleep_ms((unsigned long)ms);
    return 0;
}

i64 aster_read_key(void) {
    return (i64)keyboard_read_key();
}

i64 aster_try_read_key(void) {
    return (i64)keyboard_try_read_key();
}

i64 aster_read_line(char *buffer, i64 max_len) {
    if (!buffer || max_len <= 0) {
        return -1;
    }

    return (i64)keyboard_readline(buffer, (int)max_len);
}

i64 asterfs_create_file_sys(const char *path) {
    return (i64)asterfs_create_file(path);
}

i64 asterfs_read_file_sys(const char *path, u8 *out, u16 max_len) {
    return (i64)asterfs_read_file(path, out, max_len);
}

i64 asterfs_write_file_sys(const char *path, const u8 *data, u16 len) {
    return (i64)asterfs_write_file(path, data, len);
}

i64 asterfs_create_dir_sys(const char *path) {
    return (i64)asterfs_create_dir(path);
}

i64 asterfs_remove_file_sys(const char *path) {
    return (i64)asterfs_remove_file(path);
}

i64 asterfs_remove_dir_sys(const char *path) {
    return (i64)asterfs_remove_dir(path);
}

i64 aster_process_count_sys(void) {
    return (i64)process_count();
}

i64 aster_process_current_pid(void) {
    process_t *p = process_current();

    if (!p) {
        return -1;
    }

    return (i64)p->pid;
}

u64 aster_yield(void) {
    scheduler_yield();
    return 0;
}
