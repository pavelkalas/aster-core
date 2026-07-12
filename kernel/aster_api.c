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
#include "drivers.h"
#include "display.h"
#include "memory.h"
#include "process.h"
#include "scheduler.h"
#include "storage.h"
#include "string.h"
#include "syscall.h"
#include "timer.h"

static aster_api_dirent_cb g_list_cb = 0;
static void *g_list_user = 0;

static void aster_api_list_bridge(const char *name, u8 is_dir, u16 size) {
    aster_api_dirent_t entry;
    usize len = 0;

    if (!g_list_cb || !name) {
        return;
    }

    aster_memset(&entry, 0, sizeof(entry));
    while (name[len] && len + 1 < ASTERFS_NAME_LEN) {
        entry.name[len] = name[len];
        ++len;
    }
    entry.name[len] = '\0';
    entry.is_dir = is_dir;
    entry.size = size;
    g_list_cb(&entry, g_list_user);
}

int aster_api_context_init(aster_app_context_t *ctx) {
    if (!ctx) {
        return ASTER_API_ERR_INVALID;
    }

    ctx->api_version = ASTER_APP_API_VERSION;
    ctx->capabilities = ASTER_API_CAP_CONSOLE |
                        ASTER_API_CAP_TIME |
                        ASTER_API_CAP_MEMORY |
                        ASTER_API_CAP_INPUT |
                        ASTER_API_CAP_STORAGE |
                        ASTER_API_CAP_PROCESS;
    ctx->boot_ticks = (u64)timer_ticks();
    return ASTER_API_OK;
}

int aster_api_print(const char *text) {
    return (int)aster_write(text);
}

int aster_api_print_line(const char *text) {
    int rc;

    rc = aster_api_print(text ? text : "");
    if (rc != 0) {
        return rc;
    }

    return aster_api_print("\n");
}

int aster_api_clear(void) {
    return (int)aster_clear();
}

int aster_api_set_color(u8 fg, u8 bg) {
    display_set_color(fg, bg);
    return ASTER_API_OK;
}

int aster_api_get_cursor(usize *out_row, usize *out_col) {
    if (!out_row && !out_col) {
        return ASTER_API_ERR_INVALID;
    }

    display_get_cursor(out_row, out_col);
    return ASTER_API_OK;
}

int aster_api_set_cursor(usize row, usize col) {
    display_set_cursor(row, col);
    return ASTER_API_OK;
}

int aster_api_ticks(u64 *out_ticks) {
    if (!out_ticks) {
        return ASTER_API_ERR_INVALID;
    }

    *out_ticks = aster_api_ticks_now();
    return ASTER_API_OK;
}

u64 aster_api_ticks_now(void) {
    return aster_ticks();
}

int aster_api_sleep_ms(u32 ms) {
    return (int)aster_sleep_ms((u64)ms);
}

void *aster_api_alloc(usize size) {
    return aster_alloc(size);
}

void aster_api_memset(void *dst, u8 value, usize size) {
    if (!dst || size == 0) {
        return;
    }

    aster_memset(dst, (int)value, size);
}

void aster_api_memcpy(void *dst, const void *src, usize size) {
    if (!dst || !src || size == 0) {
        return;
    }

    aster_memcpy(dst, src, size);
}

int aster_api_memory_info(aster_api_memory_info_t *out_info) {
    if (!out_info) {
        return ASTER_API_ERR_INVALID;
    }

    out_info->total_pages = memory_total_pages();
    out_info->free_pages = memory_free_pages();
    out_info->used_pages = out_info->total_pages >= out_info->free_pages
                               ? out_info->total_pages - out_info->free_pages
                               : 0;
    return ASTER_API_OK;
}

int aster_api_read_key(void) {
    return (int)aster_read_key();
}

int aster_api_try_read_key(void) {
    return (int)aster_try_read_key();
}

int aster_api_read_line(char *buffer, int max_len) {
    return (int)aster_read_line(buffer, (i64)max_len);
}

int aster_api_file_create(const char *path) {
    return asterfs_create_file(path);
}

int aster_api_dir_create(const char *path) {
    return asterfs_create_dir(path);
}

int aster_api_file_remove(const char *path) {
    return asterfs_remove_file(path);
}

int aster_api_dir_remove(const char *path) {
    return asterfs_remove_dir(path);
}

int aster_api_path_exists(const char *path) {
    return asterfs_get_type(path) >= 0 ? 1 : 0;
}

int aster_api_file_exists(const char *path) {
    return asterfs_get_type(path) == 0 ? 1 : 0;
}

int aster_api_dir_exists(const char *path) {
    return asterfs_get_type(path) == 1 ? 1 : 0;
}

int aster_api_file_read(const char *path, u8 *out, u16 max_len) {
    return asterfs_read_file(path, out, max_len);
}

int aster_api_file_write(const char *path, const u8 *data, u16 len) {
    return asterfs_write_file(path, data, len);
}

int aster_api_file_read_text(const char *path, char *out, u16 max_len) {
    int n;

    if (!out || max_len == 0) {
        return ASTER_API_ERR_INVALID;
    }

    n = aster_api_file_read(path, (u8 *)out, (u16)(max_len - 1));
    if (n < 0) {
        out[0] = '\0';
        return n;
    }

    out[n] = '\0';
    return n;
}

int aster_api_file_write_text(const char *path, const char *text) {
    usize len;

    if (!text) {
        return ASTER_API_ERR_INVALID;
    }

    len = aster_strlen(text);
    if (len > 0xFFFFUL) {
        len = 0xFFFFUL;
    }

    return aster_api_file_write(path, (const u8 *)text, (u16)len);
}

int aster_api_list_dir(const char *path, aster_api_dirent_cb cb, void *user) {
    if (!path || !cb) {
        return ASTER_API_ERR_INVALID;
    }

    g_list_cb = cb;
    g_list_user = user;
    asterfs_list_dir(path, aster_api_list_bridge);
    g_list_cb = 0;
    g_list_user = 0;
    return ASTER_API_OK;
}

int aster_api_process_spawn(void (*entry)(void), const char *name, u8 priority) {
    return (int)aster_process_create(entry, name, priority);
}

int aster_api_process_count(usize *out_count) {
    if (!out_count) {
        return ASTER_API_ERR_INVALID;
    }

    *out_count = process_count();
    return ASTER_API_OK;
}

int aster_api_process_current(aster_api_process_info_t *out_info) {
    process_t *p;

    if (!out_info) {
        return ASTER_API_ERR_INVALID;
    }

    p = process_current();
    if (!p) {
        return ASTER_API_ERR_NOT_FOUND;
    }

    out_info->pid = p->pid;
    out_info->state = (u8)p->state;
    out_info->priority = p->priority;
    out_info->name = p->name;
    return ASTER_API_OK;
}

int aster_api_process_get(usize index, aster_api_process_info_t *out_info) {
    process_t *table;
    usize count;

    if (!out_info) {
        return ASTER_API_ERR_INVALID;
    }

    count = process_count();
    if (index >= count) {
        return ASTER_API_ERR_NOT_FOUND;
    }

    table = process_table();
    out_info->pid = table[index].pid;
    out_info->state = (u8)table[index].state;
    out_info->priority = table[index].priority;
    out_info->name = table[index].name;
    return ASTER_API_OK;
}

int aster_api_yield(void) {
    scheduler_yield();
    return ASTER_API_OK;
}

usize aster_api_str_len(const char *text) {
    return aster_strlen(text);
}

int aster_api_str_equal(const char *a, const char *b) {
    if (!a || !b) {
        return 0;
    }

    return aster_strcmp(a, b) == 0 ? 1 : 0;
}

int aster_api_str_starts_with(const char *text, const char *prefix) {
    usize n;

    if (!text || !prefix) {
        return 0;
    }

    n = aster_strlen(prefix);
    return aster_strncmp(text, prefix, n) == 0 ? 1 : 0;
}
