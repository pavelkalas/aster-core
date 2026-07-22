/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Tento soubor implementuje bridge mezi API pro aplikace a jádrem.
 * Dnes funguje jako tenká vrstva nad kernel službami a syscall API,
 * aby bylo možné na stejném rozhraní později stavět user-space runtime.
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
static int g_app_should_close = 0;

/**
 * Omezí hodnotu `v` na rozsah <min_v, max_v>.
 *
 * @param v     Vstupní hodnota (int)
 * @param min_v Minimální povolená hodnota (int)
 * @param max_v Maximální povolená hodnota (int)
 * @return      Oříznutá hodnota v daném rozsahu (int)
 */
static int clamp_i32(int v, int min_v, int max_v) {
    if (v < min_v) {
        return min_v;
    }
    if (v > max_v) {
        return max_v;
    }
    return v;
}

/**
 * Zpětné volání pro výpis adresáře – převádí údaje z AsterFS
 * do struktury `aster_api_dirent_t` a volá uživatelskou callback funkci.
 *
 * @param name   Název položky (const char *)
 * @param is_dir 1 pokud jde o adresář, 0 pokud o soubor (u8)
 * @param size   Velikost souboru v bajtech (u16)
 */
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

/**
 * Inicializuje kontext aplikace – nastaví verzi API, capabilities a boot_ticks.
 *
 * @param ctx Ukazatel na strukturu kontextu aplikace (aster_app_context_t *)
 * @return    ASTER_API_OK při úspěchu, jinak ASTER_API_ERR_INVALID
 */
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

/**
 * Vypíše text na obrazovku.
 *
 * @param text Řetězec k vypsání (const char *)
 * @return     0 při úspěchu, jinak chybový kód (int)
 */
int aster_api_print(const char *text) {
    return (int)aster_write(text);
}

/**
 * Vypíše text na obrazovku a na konec přidá nový řádek.
 *
 * @param text Řetězec k vypsání (const char *)
 * @return     0 při úspěchu, jinak chybový kód (int)
 */
int aster_api_print_line(const char *text) {
    int rc;

    rc = aster_api_print(text ? text : "");
    if (rc != 0) {
        return rc;
    }

    return aster_api_print("\n");
}

/**
 * Smaže obsah obrazovky.
 *
 * @return 0 při úspěchu (int)
 */
int aster_api_clear(void) {
    return (int)aster_clear();
}

/**
 * Nastaví barvu popředí a pozadí pro textový výstup.
 *
 * @param fg Barva popředí (u8)
 * @param bg Barva pozadí (u8)
 * @return   ASTER_API_OK
 */
int aster_api_set_color(u8 fg, u8 bg) {
    display_set_color(fg, bg);
    return ASTER_API_OK;
}

/**
 * Získá aktuální pozici kurzoru.
 *
 * @param out_row Ukazatel na proměnnou pro řádek (usize *), může být NULL
 * @param out_col Ukazatel na proměnnou pro sloupec (usize *), může být NULL
 * @return        ASTER_API_OK při úspěchu, ASTER_API_ERR_INVALID pokud jsou oba NULL
 */
int aster_api_get_cursor(usize *out_row, usize *out_col) {
    if (!out_row && !out_col) {
        return ASTER_API_ERR_INVALID;
    }

    display_get_cursor(out_row, out_col);
    return ASTER_API_OK;
}

/**
 * Nastaví kurzor na zadanou pozici.
 *
 * @param row Řádek (usize)
 * @param col Sloupec (usize)
 * @return    ASTER_API_OK
 */
int aster_api_set_cursor(usize row, usize col) {
    display_set_cursor(row, col);
    return ASTER_API_OK;
}

/**
 * Získá počet tiků od startu systému.
 *
 * @param out_ticks Ukazatel na výstupní proměnnou (u64 *)
 * @return          ASTER_API_OK při úspěchu, ASTER_API_ERR_INVALID pokud je out_ticks NULL
 */
int aster_api_ticks(u64 *out_ticks) {
    if (!out_ticks) {
        return ASTER_API_ERR_INVALID;
    }

    *out_ticks = aster_api_ticks_now();
    return ASTER_API_OK;
}

/**
 * Vrátí aktuální počet tiků od startu systému.
 *
 * @return Počet tiků (u64)
 */
u64 aster_api_ticks_now(void) {
    return aster_ticks();
}

/**
 * Pozastaví běh na zadaný počet milisekund.
 *
 * @param ms Počet milisekund ke zpoždění (u32)
 * @return   0 (u64)
 */
int aster_api_sleep_ms(u32 ms) {
    return (int)aster_sleep_ms((u64)ms);
}

/**
 * Alokuje paměť na kernelové haldě.
 *
 * @param size Požadovaná velikost v bajtech (usize)
 * @return     Ukazatel na alokovanou paměť, nebo NULL při selhání (void *)
 */
void *aster_api_alloc(usize size) {
    return aster_alloc(size);
}

/**
 * Nastaví blok paměti na zadanou hodnotu (obdoba memset).
 *
 * @param dst   Cílový ukazatel (void *)
 * @param value Hodnota, kterou se paměť vyplní (u8)
 * @param size  Počet bajtů (usize)
 */
void aster_api_memset(void *dst, u8 value, usize size) {
    if (!dst || size == 0) {
        return;
    }

    aster_memset(dst, (int)value, size);
}

/**
 * Zkopíruje blok paměti (obdoba memcpy).
 *
 * @param dst  Cílový ukazatel (void *)
 * @param src  Zdrojový ukazatel (const void *)
 * @param size Počet bajtů ke kopírování (usize)
 */
void aster_api_memcpy(void *dst, const void *src, usize size) {
    if (!dst || !src || size == 0) {
        return;
    }

    aster_memcpy(dst, src, size);
}

/**
 * Získá informace o stavu paměti (počet stránek).
 *
 * @param out_info Ukazatel na výstupní strukturu (aster_api_memory_info_t *)
 * @return         ASTER_API_OK při úspěchu, ASTER_API_ERR_INVALID při NULL
 */
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

/**
 * Přečte jednu klávesu z klávesnice (blokující).
 *
 * @return Kód stisknuté klávesy (int)
 */
int aster_api_read_key(void) {
    return (int)aster_read_key();
}

/**
 * Přečte klávesu, pokud je k dispozici (neblokující).
 *
 * @return Kód klávesy, nebo -1 pokud nic není stisknuto (int)
 */
int aster_api_try_read_key(void) {
    return (int)aster_try_read_key();
}

/**
 * Přečte řádek textu z klávesnice do bufferu.
 *
 * @param buffer  Cílový buffer (char *)
 * @param max_len Maximální délka (int)
 * @return        Počet přečtených znaků (int)
 */
int aster_api_read_line(char *buffer, int max_len) {
    return (int)aster_read_line(buffer, (i64)max_len);
}

/**
 * Vyplní obdélníkovou oblast obrazovky mezerami v dané barvě.
 *
 * @param x      Levý horní sloupec (int)
 * @param y      Levý horní řádek (int)
 * @param width  Šířka v pixelech/písmenech (int)
 * @param height Výška v pixelech/písmenech (int)
 * @param color  Barva výplně (u8)
 * @return       ASTER_API_OK při úspěchu, jinak chyba (int)
 */
int aster_api_render(int x, int y, int width, int height, u8 color) {
    char row_buf[ASTER_API_SCREEN_W + 1];
    int i;
    int yy;
    int w;
    int x0;
    int y0;
    int y1;

    if (width <= 0 || height <= 0) {
        return ASTER_API_ERR_INVALID;
    }

    x0 = clamp_i32(x, 0, ASTER_API_SCREEN_W - 1);
    y0 = clamp_i32(y, 0, ASTER_API_SCREEN_H - 1);
    y1 = clamp_i32(y + height - 1, 0, ASTER_API_SCREEN_H - 1);

    if (x >= ASTER_API_SCREEN_W || y >= ASTER_API_SCREEN_H) {
        return ASTER_API_ERR_INVALID;
    }

    w = width;
    if (x0 + w > ASTER_API_SCREEN_W) {
        w = ASTER_API_SCREEN_W - x0;
    }
    if (w <= 0) {
        return ASTER_API_ERR_INVALID;
    }

    for (i = 0; i < w; ++i) {
        row_buf[i] = ' ';
    }
    row_buf[w] = '\0';

    for (yy = y0; yy <= y1; ++yy) {
        display_write_at((usize)yy, (usize)x0, row_buf, 0x0F, color & 0x07);
    }

    return ASTER_API_OK;
}

/**
 * Vykreslí jeden znak na obrazovku na zadanou pozici s barvou.
 *
 * @param x  Sloupec (int)
 * @param y  Řádek (int)
 * @param ch Znak k vykreslení (char)
 * @param fg Barva popředí (u8)
 * @param bg Barva pozadí (u8)
 * @return   ASTER_API_OK nebo ASTER_API_ERR_INVALID (int)
 */
int aster_api_render_char(int x, int y, char ch, u8 fg, u8 bg) {
    char text[2];

    if (x < 0 || y < 0 || x >= ASTER_API_SCREEN_W || y >= ASTER_API_SCREEN_H) {
        return ASTER_API_ERR_INVALID;
    }

    text[0] = ch;
    text[1] = '\0';
    display_write_at((usize)y, (usize)x, text, fg & 0x0F, bg & 0x07);
    return ASTER_API_OK;
}

/**
 * Vykreslí textový řetězec na zadanou pozici s barvami.
 *
 * @param x          Sloupec (int)
 * @param y          Řádek (int)
 * @param text       Řetězec k vykreslení (const char *)
 * @param font_color Barva textu (u8)
 * @param back_color Barva pozadí (u8)
 * @return           ASTER_API_OK nebo ASTER_API_ERR_INVALID (int)
 */
int aster_api_render_text(int x, int y, const char *text, u8 font_color, u8 back_color) {
    if (!text) {
        return ASTER_API_ERR_INVALID;
    }

    if (x < 0 || y < 0 || x >= ASTER_API_SCREEN_W || y >= ASTER_API_SCREEN_H) {
        return ASTER_API_ERR_INVALID;
    }

    display_write_at((usize)y, (usize)x, text, font_color & 0x0F, back_color & 0x07);
    return ASTER_API_OK;
}

/**
 * Vykreslí rámeček obdélníku z ASCII znaků (+, -, |).
 *
 * @param x      Levý horní sloupec (int)
 * @param y      Levý horní řádek (int)
 * @param width  Šířka rámečku (int)
 * @param height Výška rámečku (int)
 * @param fg     Barva popředí (u8)
 * @param bg     Barva pozadí (u8)
 * @return       ASTER_API_OK nebo ASTER_API_ERR_INVALID (int)
 */
int aster_api_render_border(int x, int y, int width, int height, u8 fg, u8 bg) {
    int xx;
    int yy;
    int right;
    int bottom;

    if (width < 2 || height < 2) {
        return ASTER_API_ERR_INVALID;
    }

    right = x + width - 1;
    bottom = y + height - 1;

    for (xx = x; xx <= right; ++xx) {
        (void)aster_api_render_char(xx, y, '-', fg, bg);
        (void)aster_api_render_char(xx, bottom, '-', fg, bg);
    }

    for (yy = y; yy <= bottom; ++yy) {
        (void)aster_api_render_char(x, yy, '|', fg, bg);
        (void)aster_api_render_char(right, yy, '|', fg, bg);
    }

    (void)aster_api_render_char(x, y, '+', fg, bg);
    (void)aster_api_render_char(right, y, '+', fg, bg);
    (void)aster_api_render_char(x, bottom, '+', fg, bg);
    (void)aster_api_render_char(right, bottom, '+', fg, bg);

    return ASTER_API_OK;
}

/**
 * Vytvoří soubor na zadané cestě.
 *
 * @param path Cesta k souboru (const char *)
 * @return     0 při úspěchu, jinak chybový kód (int)
 */
int aster_api_file_create(const char *path) {
    return asterfs_create_file(path);
}

/**
 * Vytvoří adresář na zadané cestě.
 *
 * @param path Cesta k adresáři (const char *)
 * @return     0 při úspěchu, jinak chybový kód (int)
 */
int aster_api_dir_create(const char *path) {
    return asterfs_create_dir(path);
}

/**
 * Smaže soubor na zadané cestě.
 *
 * @param path Cesta k souboru (const char *)
 * @return     0 při úspěchu, jinak chybový kód (int)
 */
int aster_api_file_remove(const char *path) {
    return asterfs_remove_file(path);
}

/**
 * Smaže adresář na zadané cestě.
 *
 * @param path Cesta k adresáři (const char *)
 * @return     0 při úspěchu, jinak chybový kód (int)
 */
int aster_api_dir_remove(const char *path) {
    return asterfs_remove_dir(path);
}

/**
 * Zjistí, zda cesta existuje (soubor nebo adresář).
 *
 * @param path Cesta (const char *)
 * @return     1 pokud existuje, 0 pokud ne (int)
 */
int aster_api_path_exists(const char *path) {
    return asterfs_get_type(path) >= 0 ? 1 : 0;
}

/**
 * Zjistí, zda na dané cestě existuje soubor.
 *
 * @param path Cesta (const char *)
 * @return     1 pokud jde o soubor, jinak 0 (int)
 */
int aster_api_file_exists(const char *path) {
    return asterfs_get_type(path) == 0 ? 1 : 0;
}

/**
 * Zjistí, zda na dané cestě existuje adresář.
 *
 * @param path Cesta (const char *)
 * @return     1 pokud jde o adresář, jinak 0 (int)
 */
int aster_api_dir_exists(const char *path) {
    return asterfs_get_type(path) == 1 ? 1 : 0;
}

/**
 * Přečte obsah souboru do bufferu.
 *
 * @param path   Cesta k souboru (const char *)
 * @param out    Cílový buffer (u8 *)
 * @param max_len Maximální délka ke čtení (u16)
 * @return       Počet přečtených bajtů, nebo záporná hodnota při chybě (int)
 */
int aster_api_file_read(const char *path, u8 *out, u16 max_len) {
    return asterfs_read_file(path, out, max_len);
}

/**
 * Zapíše data do souboru.
 *
 * @param path Cesta k souboru (const char *)
 * @param data Ukazatel na data (const u8 *)
 * @param len  Délka dat v bajtech (u16)
 * @return     Počet zapsaných bajtů, nebo záporná hodnota při chybě (int)
 */
int aster_api_file_write(const char *path, const u8 *data, u16 len) {
    return asterfs_write_file(path, data, len);
}

/**
 * Přečte textový soubor a ukončí ho null-terminátorem.
 *
 * @param path    Cesta k souboru (const char *)
 * @param out     Cílový buffer (char *)
 * @param max_len Maximální délka bufferu (u16)
 * @return        Počet přečtených bajtů, nebo záporná hodnota (int)
 */
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

/**
 * Zapíše textový řetězec do souboru.
 *
 * @param path Cesta k souboru (const char *)
 * @param text Řetězec k zápisu (const char *)
 * @return     Počet zapsaných bajtů, nebo záporná hodnota (int)
 */
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

/**
 * Vypíše obsah adresáře s voláním callback funkce pro každou položku.
 *
 * @param path Cesta k adresáři (const char *)
 * @param cb   Zpětné volání pro každou položku (aster_api_dirent_cb)
 * @param user Uživatelský kontext předávaný callbacku (void *)
 * @return     ASTER_API_OK nebo ASTER_API_ERR_INVALID (int)
 */
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

/**
 * Vytvoří nový proces (vlákno) s danou vstupní funkcí.
 *
 * @param entry    Vstupní bod procesu (void (*)(void))
 * @param name     Název procesu (const char *)
 * @param priority Priorita (u8)
 * @return         PID procesu, nebo záporná hodnota při chybě (int)
 */
int aster_api_process_spawn(void (*entry)(void), const char *name, u8 priority) {
    return (int)aster_process_create(entry, name, priority);
}

/**
 * Získá celkový počet procesů v tabulce.
 *
 * @param out_count Ukazatel na výstupní proměnnou (usize *)
 * @return          ASTER_API_OK nebo ASTER_API_ERR_INVALID (int)
 */
int aster_api_process_count(usize *out_count) {
    if (!out_count) {
        return ASTER_API_ERR_INVALID;
    }

    *out_count = process_count();
    return ASTER_API_OK;
}

/**
 * Získá informace o aktuálně běžícím procesu.
 *
 * @param out_info Ukazatel na výstupní strukturu (aster_api_process_info_t *)
 * @return         ASTER_API_OK, nebo ASTER_API_ERR_NOT_FOUND / ASTER_API_ERR_INVALID (int)
 */
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

/**
 * Získá informace o procesu podle indexu v procesové tabulce.
 *
 * @param index    Index v tabulce (usize)
 * @param out_info Ukazatel na výstupní strukturu (aster_api_process_info_t *)
 * @return         ASTER_API_OK, nebo ASTER_API_ERR_NOT_FOUND / ASTER_API_ERR_INVALID (int)
 */
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

/**
 * Předá řízení plánovači (voluntary yield).
 *
 * @return ASTER_API_OK (int)
 */
int aster_api_yield(void) {
    scheduler_yield();
    return ASTER_API_OK;
}

/**
 * Vrátí délku řetězce.
 *
 * @param text Řetězec (const char *)
 * @return     Počet znaků (usize)
 */
usize aster_api_str_len(const char *text) {
    return aster_strlen(text);
}

/**
 * Porovná dva řetězce na rovnost.
 *
 * @param a První řetězec (const char *)
 * @param b Druhý řetězec (const char *)
 * @return  1 pokud jsou stejné, jinak 0 (int)
 */
int aster_api_str_equal(const char *a, const char *b) {
    if (!a || !b) {
        return 0;
    }

    return aster_strcmp(a, b) == 0 ? 1 : 0;
}

/**
 * Zjistí, zda řetězec začíná daným prefixem.
 *
 * @param text   Řetězec (const char *)
 * @param prefix Hledaný prefix (const char *)
 * @return       1 pokud ano, jinak 0 (int)
 */
int aster_api_str_starts_with(const char *text, const char *prefix) {
    usize n;

    if (!text || !prefix) {
        return 0;
    }

    n = aster_strlen(prefix);
    return aster_strncmp(text, prefix, n) == 0 ? 1 : 0;
}

/**
 * Spustí hlavní smyčku aplikace s callbacky (initial, update, draw, closing).
 * Aplikace běží, dokud nevrátí ASTER_API_APP_CLOSE nebo není požadováno zavření.
 *
 * @param callbacks  Struktura s callbacky aplikace (const aster_api_app_callbacks_t *)
 * @param user       Uživatelský kontext (void *)
 * @param target_fps Cílové FPS (u32), 0 = použije se výchozích 20
 * @return           ASTER_API_OK nebo ASTER_API_APP_ERROR (int)
 */
int aster_api_app_run(const aster_api_app_callbacks_t *callbacks, void *user, u32 target_fps) {
    aster_api_app_state_t state;
    u64 prev;

    if (!callbacks) {
        return ASTER_API_ERR_INVALID;
    }

    if (target_fps == 0) {
        target_fps = 20;
    }

    g_app_should_close = 0;

    aster_memset(&state, 0, sizeof(state));
    state.running = 1;
    state.target_fps = target_fps;
    state.ticks_now = aster_api_ticks_now();
    prev = state.ticks_now;

    if (callbacks->initial) {
        int rc = callbacks->initial(&state, user);
        if (rc == ASTER_API_APP_CLOSE) {
            state.running = 0;
        } else if (rc == ASTER_API_APP_ERROR) {
            return ASTER_API_APP_ERROR;
        }
    }

    while (state.running && !g_app_should_close) {
        int urc = ASTER_API_APP_CONTINUE;
        int drc = ASTER_API_APP_CONTINUE;
        u32 frame_ms;

        state.ticks_now = aster_api_ticks_now();
        state.dt_ticks = state.ticks_now - prev;
        prev = state.ticks_now;

        if (callbacks->update) {
            urc = callbacks->update(&state, user);
            if (urc == ASTER_API_APP_ERROR) {
                state.running = 0;
                break;
            }
            if (urc == ASTER_API_APP_CLOSE) {
                state.running = 0;
            }
        }

        if (state.running && callbacks->draw) {
            drc = callbacks->draw(&state, user);
            if (drc == ASTER_API_APP_ERROR) {
                state.running = 0;
                break;
            }
            if (drc == ASTER_API_APP_CLOSE) {
                state.running = 0;
            }
        }

        ++state.frame;
        frame_ms = (u32)(1000U / (state.target_fps ? state.target_fps : 1U));
        if (frame_ms == 0) {
            frame_ms = 1;
        }
        (void)aster_api_sleep_ms(frame_ms);
    }

    if (callbacks->closing) {
        (void)callbacks->closing(&state, user);
    }

    return ASTER_API_OK;
}

/**
 * Požádá o ukončení běžící aplikace (nastaví příznak).
 */
void aster_api_app_request_close(void) {
    g_app_should_close = 1;
}
