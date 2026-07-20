/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Bootlog — jednotne stavove vypisy, boot stepy a shutdown priprava.
 * Sjednocuje vsechny logovaci vypisy jadra na jednom miste.
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

usize append_text(char *dst, usize pos, usize max, const char *src) {
    while (*src && pos + 1 < max) {
        dst[pos++] = *src++;
    }
    if (max > 0) {
        dst[pos < max ? pos : max - 1] = '\0';
    }
    return pos;
}

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

int boot_splash_is_active(void) {
    return g_boot_splash_active;
}

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

void boot_splash_finish(void) {
    g_boot_splash_active = 0;
}

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

void bootlog_state(const char *state, unsigned char state_color, const char *msg) {
    display_set_color(0x0F, 0x00);
    aster_print("[ ");
    display_set_color(state_color, 0x00);
    aster_print(state ? state : "NEZNAMY");
    display_set_color(0x0F, 0x00);
    aster_print(" ] ");
    printk("%s\n", msg ? msg : "");
}

void bootlog_ok(const char *msg) {
    bootlog_state("HOTOVO", 0x0A, msg);
}

void bootlog_error(const char *msg) {
    bootlog_state("CHYBA", 0x0C, msg);
}

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

void boot_step_ok(const char *label) {
    boot_step_finish(label, "Okej", 0x0A);
}

void boot_step_skip(const char *label) {
    boot_step_finish(label, "SKIP", 0x0E);
}

#include "auth.h"
#include "keyboard.h"
#include "statusbar.h"

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
