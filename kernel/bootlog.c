/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Bootlog — jednotné stavové výpisy, boot stepy a shutdown příprava.
 * Sjednocuje všechny logovací výpisy jádra na jednom místě.
 */

#include "bootlog.h"

#include "auth.h"
#include "display.h"
#include "drivers.h"
#include "printk.h"
#include "statusbar.h"
#include "storage.h"
#include "string.h"
#include "timer.h"

/**
 * Přidá textový řetězec na konec bufferu.
 *
 * @param dst   Cílový buffer (char *)
 * @param pos   Aktuální pozice v bufferu (usize)
 * @param max   Maximální velikost bufferu (usize)
 * @param src   Zdrojový řetězec (const char *)
 * @return      Nová pozice v bufferu (usize)
 */
usize append_text(char *dst, usize pos, usize max, const char *src) {
    while (*src && pos + 1 < max) {
        dst[pos++] = *src++;
    }
    if (max > 0) {
        dst[pos < max ? pos : max - 1] = '\0';
    }
    return pos;
}

/**
 * Přidá celé číslo (unsigned) na konec bufferu jako dekadický text.
 *
 * @param dst Cílový buffer (char *)
 * @param pos Aktuální pozice v bufferu (usize)
 * @param max Maximální velikost bufferu (usize)
 * @param v   Hodnota k přidání (unsigned int)
 * @return    Nová pozice v bufferu (usize)
 */
usize append_uint(char *dst, usize pos, usize max, unsigned int v) {
    char tmp[12];
    int i = 0;

    if (v == 0) {
        if (pos + 1 < max) {
            dst[pos++] = '0';
            dst[pos] = '\0';
        }
        return pos;
    }

    while (v > 0 && i < (int)sizeof(tmp)) {
        tmp[i++] = (char)('0' + (v % 10));
        v /= 10;
    }

    while (i > 0 && pos + 1 < max) {
        dst[pos++] = tmp[--i];
    }

    dst[pos] = '\0';
    return pos;
}

#define BOOT_SPLASH_LOG_COUNT_MAX 4U

static int g_boot_splash_active = 0;
static unsigned int g_boot_splash_steps_total = 0;
static unsigned int g_boot_splash_steps_done = 0;
static char g_boot_splash_log[BOOT_SPLASH_LOG_COUNT_MAX][56];
static unsigned int g_boot_splash_log_count = 0;

/**
 * Zjistí, zda je bootovací splash obrazovka aktivní.
 *
 * @return 1 pokud je aktivní, jinak 0 (int)
 */
int boot_splash_is_active(void) {
    return g_boot_splash_active;
}

/**
 * Resetuje splash obrazovku pro daný počet kroků.
 *
 * @param steps_total Celkový počet boot kroků (unsigned int)
 */
void boot_splash_reset(unsigned int steps_total) {
    unsigned int i;

    g_boot_splash_active = 1;
    g_boot_splash_steps_total = steps_total > 0 ? steps_total : 1;
    g_boot_splash_steps_done = 0;

    for (i = 0; i < BOOT_SPLASH_LOG_COUNT_MAX; ++i) {
        g_boot_splash_log[i][0] = '\0';
    }
    g_boot_splash_log_count = 0;
}

/**
 * Ukončí splash obrazovku.
 */
void boot_splash_finish(void) {
    g_boot_splash_active = 0;
}

/**
 * Vykreslí log posledních boot kroků na splash obrazovku.
 */
static void boot_splash_render_log(void) {
    unsigned int i;

    display_write_at(13, 20, "Step log (array=4):", 0x0F, 0x00);
    for (i = 0; i < BOOT_SPLASH_LOG_COUNT_MAX; ++i) {
        display_write_at(14 + i, 20, "                                                        ", 0x0F, 0x00);
        if (i < g_boot_splash_log_count) {
            display_write_at(14 + i, 20, g_boot_splash_log[i], 0x0F, 0x00);
        }
    }
}

/**
 * Přidá zprávu do logu splash obrazovky (posouvá staré zprávy).
 *
 * @param text Text zprávy (const char *)
 */
static void boot_splash_push_log(const char *text) {
    unsigned int i;
    usize n = 0;

    if (!text) {
        return;
    }

    if (g_boot_splash_log_count < BOOT_SPLASH_LOG_COUNT_MAX) {
        i = g_boot_splash_log_count;
        ++g_boot_splash_log_count;
    } else {
        for (i = 1; i < BOOT_SPLASH_LOG_COUNT_MAX; ++i) {
            aster_memcpy(g_boot_splash_log[i - 1], g_boot_splash_log[i], sizeof(g_boot_splash_log[0]));
        }
        i = BOOT_SPLASH_LOG_COUNT_MAX - 1;
    }

    while (text[n] && n + 1 < sizeof(g_boot_splash_log[0])) {
        g_boot_splash_log[i][n] = text[n];
        ++n;
    }
    g_boot_splash_log[i][n] = '\0';

    boot_splash_render_log();
}

/**
 * Aktualizuje splash obrazovku – progress bar a log.
 *
 * @param label       Název kroku (const char *)
 * @param state       Text stavu ("Okej", "SKIP", ...) (const char *)
 * @param state_color Barva stavu (u8)
 */
static void boot_splash_update(const char *label, const char *state, u8 state_color) {
    const usize bar_w = 34;
    char bar[36];
    char log_line[56];
    char pct[8];
    unsigned int i;
    unsigned int percent;
    usize fill;
    usize p = 0;

    if (!g_boot_splash_active) {
        return;
    }

    (void)state_color;

    if (g_boot_splash_steps_done < g_boot_splash_steps_total) {
        ++g_boot_splash_steps_done;
    }

    p = append_text(log_line, 0, sizeof(log_line), "[");
    p = append_text(log_line, p, sizeof(log_line), state ? state : "?");
    p = append_text(log_line, p, sizeof(log_line), "] ");
    p = append_text(log_line, p, sizeof(log_line), label ? label : "unknown");
    log_line[p] = '\0';
    boot_splash_push_log(log_line);

    percent = (g_boot_splash_steps_done * 100U) / g_boot_splash_steps_total;
    fill = (usize)((g_boot_splash_steps_done * bar_w) / g_boot_splash_steps_total);

    for (i = 0; i < bar_w; ++i) {
        bar[i] = ((usize)i < fill) ? '#' : '-';
    }
    bar[bar_w] = '\0';

    display_write_at(18, 21, bar, 0x0A, 0x00);

    p = append_uint(pct, 0, sizeof(pct), percent);
    if (p + 1 < sizeof(pct)) {
        pct[p++] = '%';
        pct[p] = '\0';
    }

    display_write_at(19, 34, "    ", 0x0F, 0x00);
    display_write_at(19, 34, pct, 0x0F, 0x00);
}

/**
 * Vypíše stavovou zprávu ve formátu [ STAV ] zpráva.
 *
 * @param state       Text stavu (const char *)
 * @param state_color Barva stavu (unsigned char)
 * @param msg         Zpráva (const char *)
 */
void bootlog_state(const char *state, unsigned char state_color, const char *msg) {
    display_set_color(0x0F, 0x00);
    aster_print("[ ");
    display_set_color(state_color, 0x00);
    aster_print(state ? state : "NEZNAMY");
    display_set_color(0x0F, 0x00);
    aster_print(" ] ");
    printk("%s\n", msg ? msg : "");
}

/**
 * Vypíše stav "HOTOVO" (zeleně) se zprávou.
 *
 * @param msg Zpráva (const char *)
 */
void bootlog_ok(const char *msg) {
    bootlog_state("HOTOVO", 0x0A, msg);
}

/**
 * Vypíše stav "CHYBA" (červeně) se zprávou.
 *
 * @param msg Zpráva (const char *)
 */
void bootlog_error(const char *msg) {
    bootlog_state("CHYBA", 0x0C, msg);
}

/**
 * Zahájí boot krok – vypíše jeho název.
 * Pokud je aktivní splash, pouze přidá záznam do logu.
 *
 * @param label Název kroku (const char *)
 */
void boot_step_begin(const char *label) {
    if (g_boot_splash_active) {
        char log_line[56];
        usize p = append_text(log_line, 0, sizeof(log_line), "[ *** ] ");
        p = append_text(log_line, p, sizeof(log_line), label ? label : "unknown");
        log_line[p] = '\0';
        boot_splash_push_log(log_line);
        return;
    }
    display_set_color(0x0F, 0x00);
    printk("[ *** ] %s", label);

    timer_sleep_ms(100);
}

/**
 * Dokončí boot krok – přepíše řádek na [stav] název.
 * Pokud je aktivní splash, aktualizuje progress bar.
 *
 * @param label       Název kroku (const char *)
 * @param state       Text stavu (const char *)
 * @param state_color Barva stavu (u8)
 */
void boot_step_finish(const char *label, const char *state, u8 state_color) {
    if (g_boot_splash_active) {
        boot_splash_update(label, state, state_color);
        return;
    }
    aster_print("\r");
    display_set_color(0x0F, 0x00);
    aster_print("[ ");
    display_set_color(state_color, 0x00);
    aster_print(state);
    display_set_color(0x0F, 0x00);
    printk(" ] %s\n", label);
}

/**
 * Dokončí boot krok se stavem "Okej" (zeleně).
 *
 * @param label Název kroku (const char *)
 */
void boot_step_ok(const char *label) {
    boot_step_finish(label, "Okej", 0x0A);
}

/**
 * Dokončí boot krok se stavem "SKIP" (žlutě).
 *
 * @param label Název kroku (const char *)
 */
void boot_step_skip(const char *label) {
    boot_step_finish(label, "SKIP", 0x0E);
}

#include "auth.h"
#include "keyboard.h"
#include "statusbar.h"

/**
 * Připraví systém na vypnutí nebo restart.
 * Ukončí session, odebere moduly, uvolní paměť a synchronizuje FS.
 *
 * @param typ Řetězec popisující typ ukončení ("restart", "vypnuti", ...) (const char *)
 */
void system_shutdown_prepare(const char *typ) {
    if (typ && typ[0]) {
        display_clear();
        display_set_color(0x0F, 0x00);
        aster_print("Sekvence ");
        aster_print(typ);
        aster_print(" systemu\n\n");
    }

    boot_step_begin("Ukonceni uzivatelske session");
    {
        extern usize g_current_user_size;
        aster_memset(g_current_user, 0, g_current_user_size);
    }
    boot_step_ok("Ukonceni uzivatelske session");

    boot_step_begin("Odebirani modulu");
    keyboard_set_refresh_callback(0, 0);
    boot_step_ok("Odebirani modulu");

    boot_step_begin("Uvolnovani pameti");
    g_status_marquee_only = 0;
    status_clear_marquee();
    status_clear_left_hint();
    boot_step_ok("Uvolnovani pameti");

    boot_step_begin("Synchronizace souboroveho systemu");
    if (asterfs_sync() == 0) {
        boot_step_ok("Synchronizace souboroveho systemu");
    } else {
        boot_step_finish("Synchronizace souboroveho systemu", "CHYBA", 0x0C);
    }

    timer_sleep_ms(1000);
}
