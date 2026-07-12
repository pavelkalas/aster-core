/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header popisuje syscall ABI mezi user casti a jadrem.
 * Definuje cisla volani, centralni dispatcher a obalove funkce,
 * ktere mapuji uzivatelske pozadavky na kernelove sluzby.
 */

#ifndef ASTER_SYSCALL_H
#define ASTER_SYSCALL_H

#include "types.h"

#define SYSCALL_WRITE          0
#define SYSCALL_ALLOC          1
#define SYSCALL_PROCESS_CREATE 2
#define SYSCALL_CLEAR          3
#define SYSCALL_TICKS          4
#define SYSCALL_SLEEP_MS       5
#define SYSCALL_READ_KEY       6
#define SYSCALL_TRY_READ_KEY   7
#define SYSCALL_READ_LINE      8
#define SYSCALL_FS_CREATE_FILE 9
#define SYSCALL_FS_READ_FILE   10
#define SYSCALL_FS_WRITE_FILE  11
#define SYSCALL_FS_CREATE_DIR  12
#define SYSCALL_FS_REMOVE_FILE 13
#define SYSCALL_FS_REMOVE_DIR  14
#define SYSCALL_PROCESS_COUNT  15
#define SYSCALL_PROCESS_CURPID 16
#define SYSCALL_YIELD          17

void syscall_init(void);
u64 syscall_dispatch(u64 id, u64 a1, u64 a2, u64 a3, u64 a4);

u64 aster_write(const char *text);
void *aster_alloc(usize size);
i64 aster_process_create(void (*entry)(void), const char *name, u8 priority);
u64 aster_clear(void);
u64 aster_ticks(void);
u64 aster_sleep_ms(u64 ms);
i64 aster_read_key(void);
i64 aster_try_read_key(void);
i64 aster_read_line(char *buffer, i64 max_len);
i64 asterfs_create_file_sys(const char *path);
i64 asterfs_read_file_sys(const char *path, u8 *out, u16 max_len);
i64 asterfs_write_file_sys(const char *path, const u8 *data, u16 len);
i64 asterfs_create_dir_sys(const char *path);
i64 asterfs_remove_file_sys(const char *path);
i64 asterfs_remove_dir_sys(const char *path);
i64 aster_process_count_sys(void);
i64 aster_process_current_pid(void);
u64 aster_yield(void);

#endif
