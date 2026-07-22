/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Tento header definuje experimentální API vrstvu pro C aplikace.
 * API je navrženo pro budoucí user-space runtime, ale lze ho už teď
 * použít pro jednotné volání kernel služeb v interních aplikacích.
 */

#ifndef ASTER_API_H
#define ASTER_API_H

#include "types.h"
#include "storage.h"

/** Verze API používaná aplikacemi. */
#define ASTER_APP_API_VERSION 2

/** Capability masky pro konzolové API. */
#define ASTER_API_CAP_CONSOLE  (1U << 0)
#define ASTER_API_CAP_TIME     (1U << 1)
#define ASTER_API_CAP_MEMORY   (1U << 2)
#define ASTER_API_CAP_INPUT    (1U << 3)
#define ASTER_API_CAP_STORAGE  (1U << 4)
#define ASTER_API_CAP_PROCESS  (1U << 5)

/** Rozměry obrazovky (znaků). */
#define ASTER_API_SCREEN_W 80
#define ASTER_API_SCREEN_H 25

/** Návratové hodnoty callbacků aplikace. */
#define ASTER_API_APP_CONTINUE 0
#define ASTER_API_APP_CLOSE    1
#define ASTER_API_APP_ERROR   -1

/** Konstanty pro speciální klávesy (F1, F2, šipky). */
#define ASTER_API_KEY_F1     0x101
#define ASTER_API_KEY_F2     0x102
#define ASTER_API_KEY_UP     0x103
#define ASTER_API_KEY_DOWN   0x104
#define ASTER_API_KEY_LEFT   0x105
#define ASTER_API_KEY_RIGHT  0x106

/** Chybové kódy API. */
#define ASTER_API_OK                0
#define ASTER_API_ERR_INVALID      -1
#define ASTER_API_ERR_NOT_FOUND    -2
#define ASTER_API_ERR_NO_SPACE     -3
#define ASTER_API_ERR_ALREADY      -4

/** Struktura s informacemi o paměti. */
typedef struct {
    usize total_pages; /**< Celkový počet stránek. */
    usize free_pages;  /**< Počet volných stránek. */
    usize used_pages;  /**< Počet použitých stránek. */
} aster_api_memory_info_t;

/** Struktura s informacemi o procesu. */
typedef struct {
    u32 pid;               /**< PID procesu. */
    u8 state;              /**< Stav procesu. */
    u8 priority;           /**< Priorita procesu. */
    const char *name;      /**< Ukazatel na název procesu. */
} aster_api_process_info_t;

/** Struktura položky adresáře. */
typedef struct {
    char name[ASTERFS_NAME_LEN]; /**< Název položky. */
    u8 is_dir;                   /**< 1 pokud jde o adresář, jinak 0. */
    u16 size;                    /**< Velikost souboru v bajtech. */
} aster_api_dirent_t;

/** Stav aplikace předávaný callbackům. */
typedef struct {
    u64 frame;       /**< Aktuální číslo snímku. */
    u64 ticks_now;   /**< Aktuální počet tiků. */
    u64 dt_ticks;    /**< Počet tiků od posledního snímku. */
    int running;     /**< 1 pokud aplikace běží, 0 pokud má skončit. */
    u32 target_fps;  /**< Cílové FPS. */
} aster_api_app_state_t;

/** Callbacky pro hlavní smyčku aplikace. */
typedef struct {
    int (*initial)(aster_api_app_state_t *state, void *user);  /**< Inicializace. */
    int (*update)(aster_api_app_state_t *state, void *user);   /**< Aktualizace logiky. */
    int (*draw)(aster_api_app_state_t *state, void *user);     /**< Vykreslení. */
    int (*closing)(aster_api_app_state_t *state, void *user);  /**< Ukončení. */
} aster_api_app_callbacks_t;

/** Typ pro zpětné volání při výpisu adresáře. */
typedef void (*aster_api_dirent_cb)(const aster_api_dirent_t *entry, void *user);

/** Kontext aplikace – verze, capabilities, boot ticks. */
typedef struct {
    u32 api_version;   /**< Verze API. */
    u32 capabilities;  /**< Bitová maska capabilities. */
    u64 boot_ticks;    /**< Počet tiků při startu. */
} aster_app_context_t;

/** Inicializuje kontext aplikace. */
int aster_api_context_init(aster_app_context_t *ctx);

/** Konzolové API – výpis, čištění, kurzor. */
int aster_api_print(const char *text);
int aster_api_print_line(const char *text);
int aster_api_clear(void);
int aster_api_set_color(u8 fg, u8 bg);
int aster_api_get_cursor(usize *out_row, usize *out_col);
int aster_api_set_cursor(usize row, usize col);

/** Časové API – ticky a uspání. */
int aster_api_ticks(u64 *out_ticks);
u64 aster_api_ticks_now(void);
int aster_api_sleep_ms(u32 ms);

/** Paměťové API a utility. */
void *aster_api_alloc(usize size);
void aster_api_memset(void *dst, u8 value, usize size);
void aster_api_memcpy(void *dst, const void *src, usize size);
int aster_api_memory_info(aster_api_memory_info_t *out_info);

/** Vstupní API pro klávesnici. */
int aster_api_read_key(void);
int aster_api_try_read_key(void);
int aster_api_read_line(char *buffer, int max_len);

/** Render API pro VGA textový framebuffer. */
int aster_api_render(int x, int y, int width, int height, u8 color);
int aster_api_render_char(int x, int y, char ch, u8 fg, u8 bg);
int aster_api_render_text(int x, int y, const char *text, u8 font_color, u8 back_color);
int aster_api_render_border(int x, int y, int width, int height, u8 fg, u8 bg);

/** AsterFS API. */
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

/** Procesové API. */
int aster_api_process_spawn(void (*entry)(void), const char *name, u8 priority);
int aster_api_process_count(usize *out_count);
int aster_api_process_current(aster_api_process_info_t *out_info);
int aster_api_process_get(usize index, aster_api_process_info_t *out_info);
int aster_api_yield(void);

/** String utility API. */
usize aster_api_str_len(const char *text);
int aster_api_str_equal(const char *a, const char *b);
int aster_api_str_starts_with(const char *text, const char *prefix);

/** Lifecycle runtime pro aplikace. */
int aster_api_app_run(const aster_api_app_callbacks_t *callbacks, void *user, u32 target_fps);
void aster_api_app_request_close(void);

#endif
