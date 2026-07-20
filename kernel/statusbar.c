/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 *
 * Statusbar – spodni radek obrazovky s pameti, marquee a hinty.
 */

#include "statusbar.h"
#include "bootlog.h"
#include "display.h"
#include "memory.h"
#include "printk.h"

#define SCREEN_W 80
#define SCREEN_H 25

char g_status_marquee[160];
int  g_status_marquee_on = 0;
usize g_status_marquee_offset = 0;
int  g_status_marquee_only = 0;
char g_status_left_hint[80];
int  g_status_left_hint_on = 0;

void status_set_marquee(const char *text) {
    usize p = 0;
    usize i = 0;

    if (!text) {
        g_status_marquee[0] = '\0';
        g_status_marquee_on = 0;
        g_status_marquee_offset = 0;
        return;
    }

    while (text[i] && p + 1 < sizeof(g_status_marquee)) {
        g_status_marquee[p++] = text[i++];
    }

    for (i = 0; i < 10 && p + 1 < sizeof(g_status_marquee); ++i) {
        g_status_marquee[p++] = ' ';
    }

    g_status_marquee[p] = '\0';
    g_status_marquee_on = (p > 0) ? 1 : 0;
    g_status_marquee_offset = 0;
}

void status_clear_marquee(void) {
    g_status_marquee[0] = '\0';
    g_status_marquee_on = 0;
    g_status_marquee_offset = 0;
}

void status_set_left_hint(const char *text) {
    usize i = 0;

    if (!text) {
        g_status_left_hint[0] = '\0';
        g_status_left_hint_on = 0;
        return;
    }

    while (text[i] && i + 1 < sizeof(g_status_left_hint)) {
        g_status_left_hint[i] = text[i];
        ++i;
    }
    g_status_left_hint[i] = '\0';
    g_status_left_hint_on = (i > 0) ? 1 : 0;
}

void status_clear_left_hint(void) {
    g_status_left_hint[0] = '\0';
    g_status_left_hint_on = 0;
}

void render_shell_statusbar(void) {
    usize save_row;
    usize save_col;
    usize total = memory_total_pages();
    usize freep = memory_free_pages();
    usize used = total >= freep ? total - freep : 0;
    char left[64];
    char right[64];
    char middle[64];
    usize rlen;
    usize llen;
    usize mlen;
    usize i;
    usize right_start;
    usize middle_start;

    display_get_cursor(&save_row, &save_col);

    left[0] = '\0';
    right[0] = '\0';
    middle[0] = '\0';

    display_fill_row(SCREEN_H - 1, ' ', 0, 0);

    if (!g_status_marquee_only) {
        usize p = 0;
        p = append_text(left, p, sizeof(left), "pamet: strankovani=");
        p = append_uint(left, p, sizeof(left), (unsigned int)total);
        p = append_text(left, p, sizeof(left), " volne=");
        p = append_uint(left, p, sizeof(left), (unsigned int)freep);
        p = append_text(left, p, sizeof(left), " pouzite=");
        p = append_uint(left, p, sizeof(left), (unsigned int)used);
        (void)p;
        display_write_at(SCREEN_H - 1, 0, left, 0x0E, 0);
    } else if (g_status_left_hint_on) {
        display_write_at(SCREEN_H - 1, 0, g_status_left_hint, 0x0E, 0);
        llen = 0;
        while (g_status_left_hint[llen]) ++llen;
    }

    right[0] = '\0';
    rlen = 0;
    while (right[rlen]) ++rlen;
    if (!g_status_marquee_only) {
        llen = 0;
        while (left[llen]) ++llen;
    }

    if (g_status_marquee_on) {
        usize src_len = 0;
        usize view_len = 22;
        usize s;
        while (g_status_marquee[src_len]) ++src_len;
        if (view_len + 1 > sizeof(middle))
            view_len = sizeof(middle) - 1;
        if (src_len > 0) {
            s = g_status_marquee_offset % src_len;
            for (i = 0; i < view_len; ++i)
                middle[i] = g_status_marquee[(s + i) % src_len];
            middle[view_len] = '\0';
        }
    }

    mlen = 0;
    while (middle[mlen]) ++mlen;
    if (mlen > 0) {
        middle_start = (SCREEN_W > mlen) ? ((SCREEN_W - mlen) / 2) : 0;
        if (g_status_marquee_only) {
            if (middle_start < llen + 6)
                middle_start = llen + 6;
        } else if (middle_start < llen + 1)
            middle_start = llen + 1;
        if (middle_start + mlen + 1 < SCREEN_W)
            display_write_at(SCREEN_H - 1, middle_start, middle, 0x0E, 0);
    }

    if (!g_status_marquee_only) {
        right_start = (rlen < SCREEN_W) ? (SCREEN_W - rlen) : 0;
        display_write_at(SCREEN_H - 1, right_start, right, 0x0E, 0);
    }

    if (save_row >= SCREEN_H - 1)
        save_row = SCREEN_H - 2;
    display_set_cursor(save_row, save_col);
}
