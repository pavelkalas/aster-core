/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Tento header definuje experimentalni API vrstvu pro C aplikace.
 * API je navrzene pro budouci user-space runtime, ale lze ho uz ted
 * pouzit pro jednotne volani kernel sluzeb v internich aplikacich.
 */

#ifndef ASTER_API_H
#define ASTER_API_H

#include "types.h"
#include "storage.h"

#define ASTER_APP_API_VERSION 2

#define ASTER_API_CAP_CONSOLE  (1U << 0)
#define ASTER_API_CAP_TIME     (1U << 1)
#define ASTER_API_CAP_MEMORY   (1U << 2)
#define ASTER_API_CAP_INPUT    (1U << 3)
#define ASTER_API_CAP_STORAGE  (1U << 4)
#define ASTER_API_CAP_PROCESS  (1U << 5)

#define ASTER_API_OK                0
#define ASTER_API_ERR_INVALID      -1
#define ASTER_API_ERR_NOT_FOUND    -2
#define ASTER_API_ERR_NO_SPACE     -3
#define ASTER_API_ERR_ALREADY      -4

typedef struct {
    usize total_pages;
    usize free_pages;
    usize used_pages;
} aster_api_memory_info_t;

typedef struct {
    u32 pid;
    u8 state;
    u8 priority;
    const char *name;
} aster_api_process_info_t;

typedef struct {
    char name[ASTERFS_NAME_LEN];
    u8 is_dir;
    u16 size;
} aster_api_dirent_t;

typedef void (*aster_api_dirent_cb)(const aster_api_dirent_t *entry, void *user);

typedef struct {
    u32 api_version;
    u32 capabilities;
    u64 boot_ticks;
} aster_app_context_t;

/*
 * Inicializuje kontext aplikace a vyplni verzi API, capability masku
 * a aktualni hodnotu boot tiku. Vraci ASTER_API_OK nebo chybu.
 */
int aster_api_context_init(aster_app_context_t *ctx);

/*
 * Konzolove API pro vypis textu, cisteni obrazovky a praci s kurzorem.
 */
int aster_api_print(const char *text);
int aster_api_print_line(const char *text);
int aster_api_clear(void);
int aster_api_set_color(u8 fg, u8 bg);
int aster_api_get_cursor(usize *out_row, usize *out_col);
int aster_api_set_cursor(usize row, usize col);

/*
 * Casove API. Tick je globalni casovac jadra od bootu.
 */
int aster_api_ticks(u64 *out_ticks);
u64 aster_api_ticks_now(void);
int aster_api_sleep_ms(u32 ms);

/*
 * Pametove API a jednoduche utility nad pameti bez zavislosti na libc.
 */
void *aster_api_alloc(usize size);
void aster_api_memset(void *dst, u8 value, usize size);
void aster_api_memcpy(void *dst, const void *src, usize size);
int aster_api_memory_info(aster_api_memory_info_t *out_info);

/*
 * Vstupni API pro klavesnici.
 * read_key blokuje, try_read_key je neblokujici, read_line cte cely radek.
 */
int aster_api_read_key(void);
int aster_api_try_read_key(void);
int aster_api_read_line(char *buffer, int max_len);

/*
 * AsterFS API pokryvajici bezne operace nad soubory a adresari.
 * Textove helpery zajistuji nulovy terminator pri cteni textu.
 */
int aster_api_file_create(const char *path);
int aster_api_dir_create(const char *path);
int aster_api_file_remove(const char *path);
int aster_api_dir_remove(const char *path);
int aster_api_path_exists(const char *path);
int aster_api_file_exists(const char *path);
int aster_api_dir_exists(const char *path);
int aster_api_file_read(const char *path, u8 *out, u16 max_len);
int aster_api_file_write(const char *path, const u8 *data, u16 len);
int aster_api_file_read_text(const char *path, char *out, u16 max_len);
int aster_api_file_write_text(const char *path, const char *text);
int aster_api_list_dir(const char *path, aster_api_dirent_cb cb, void *user);

/*
 * Procesove API pro spawn, introspekci tabulky procesu a voluntary yield.
 */
int aster_api_process_spawn(void (*entry)(void), const char *name, u8 priority);
int aster_api_process_count(usize *out_count);
int aster_api_process_current(aster_api_process_info_t *out_info);
int aster_api_process_get(usize index, aster_api_process_info_t *out_info);
int aster_api_yield(void);

/*
 * String utility API urcene pro aplikace bez standardni knihovny C.
 */
usize aster_api_str_len(const char *text);
int aster_api_str_equal(const char *a, const char *b);
int aster_api_str_starts_with(const char *text, const char *prefix);

#endif
