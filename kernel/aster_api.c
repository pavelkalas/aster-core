/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Tento soubor implementuje bridge mezi API pro aplikace a jadrem.
 * Dnes funguje jako tenka vrstva nad kernel sluzbami a syscall API,
 * aby bylo mozne na stejnem rozhrani pozdeji stavet user-space runtime.
 */

#include "aster_api.h"
#include "display.h"
#include "storage.h"
#include "syscall.h"
#include "timer.h"

int aster_api_print(const char *text) {
    return (int)aster_write(text);
}

int aster_api_clear(void) {
    display_clear();
    return 0;
}

int aster_api_ticks(u64 *out_ticks) {
    if (!out_ticks) {
        return -1;
    }

    *out_ticks = (u64)timer_ticks();
    return 0;
}

void *aster_api_alloc(usize size) {
    return aster_alloc(size);
}

int aster_api_file_create(const char *path) {
    return asterfs_create_file(path);
}

int aster_api_file_read(const char *path, u8 *out, u16 max_len) {
    return asterfs_read_file(path, out, max_len);
}

int aster_api_file_write(const char *path, const u8 *data, u16 len) {
    return asterfs_write_file(path, data, len);
}

int aster_api_process_spawn(void (*entry)(void), const char *name, u8 priority) {
    return (int)aster_process_create(entry, name, priority);
}
