/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 *
 * Textový editor pro shell.
 */

#include "editor.h"
#include "bootlog.h"
#include "display.h"
#include "drivers.h"
#include "printk.h"
#include "statusbar.h"
#include "storage.h"
#include "string.h"
#include "timer.h"
#include "fs_utils.h"

/**
 * Překreslí celou obrazovku editoru včetně obsahu souboru a kurzoru.
 *
 * @param path Cesta k souboru (const char *)
 * @param buf  Buffer s obsahem souboru (const char *)
 * @param len  Délka obsahu (usize)
 * @param pos  Pozice kurzoru v bufferu (usize)
 */
static void editor_redraw(const char *path, const char *buf, usize len, usize pos) {
    usize i;
    usize line = 1;
    usize cursor_row = 0, cursor_col = 0;
    int cursor_set = 0;

    for (i = 0; i < pos && i < len; ++i)
        if (buf[i] == '\n') ++line;

    display_set_color(0x0F, 0x00);
    display_clear();
    display_set_color(0x0F, 0x00);
    printk("EDITOR | file: %s | line: %u", path, (unsigned int)line);
    display_set_color(0x0F, 0x00);
    aster_print("\n\n");

    for (i = 0; i < len; ++i) {
        if (!cursor_set && i == pos) {
            display_get_cursor(&cursor_row, &cursor_col);
            cursor_set = 1;
        }
        display_putc(buf[i]);
    }
    if (!cursor_set) display_get_cursor(&cursor_row, &cursor_col);

    aster_print("\n\n");
    display_set_color(0x0F, 0x00);
    render_shell_statusbar();
    display_set_cursor(cursor_row, cursor_col);
}

/**
 * Spustí textový editor pro soubor na zadané cestě.
 * Podporuje pohyb šipkami, mazání Backspace, ukládání (Ctrl+S)
 * a ukončení (ESC).
 *
 * @param path Cesta k souboru (const char *)
 */
void shell_edit_file(const char *path) {
    char buf[4096];
    int n = asterfs_read_file(path, (u8 *)buf, 4096);
    usize len;
    usize pos;
    unsigned long last_anim = timer_ticks();
    unsigned long tick_counter = 0;
    unsigned long step_ticks = 1;
    unsigned long shift_interval = 10;
    char title[120];
    usize tp = 0;
    const char *prefix = "* Upravovani souboru:";
    const char *base = path_basename(path);
    usize bi = 0;

    while (prefix[tp] && tp + 1 < sizeof(title)) title[tp++] = prefix[tp];
    while (base[bi] && tp + 1 < sizeof(title)) title[tp++] = base[bi++];
    title[tp] = '\0';

    if (n < 0) {
        if (asterfs_create_file(path) != 0) {
            aster_print("Editor: nelze vytvorit soubor\n");
            return;
        }
        n = 0;
    }
    len = (usize)n; buf[len] = '\0'; pos = len;
    g_status_marquee_only = 1;
    status_set_left_hint("Ctrl+S = ulozit | ESC = uzavrit");
    status_set_marquee(title);
    editor_redraw(path, buf, len, pos);

    for (;;) {
        int key;
        unsigned long now = timer_ticks();
        unsigned long delta;
        key = keyboard_try_read_key();

        if (now > last_anim) {
            delta = now - last_anim;
            if (delta >= step_ticks) {
                tick_counter += delta;
                if (tick_counter >= shift_interval) {
                    g_status_marquee_offset++;
                    tick_counter = 0;
                    render_shell_statusbar();
                }
                last_anim = now;
            }
        }
        if (key == -1) { __asm__ volatile ("pause"); continue; }

        if (key == 27) {
            g_status_marquee_only = 0;
            status_clear_left_hint(); status_clear_marquee();
            display_clear(); return;
        }
        if (key == 19) {
            (void)asterfs_write_file(path, (const u8 *)buf, (u16)len);
            display_set_color(0x0F, 0x00);
            aster_print("Saved\n");
            editor_redraw(path, buf, len, pos);
            continue;
        }
        if (key == '\b') {
            if (len > 0 && pos > 0) {
                usize j;
                for (j = pos - 1; j + 1 < len; ++j) buf[j] = buf[j + 1];
                --len; --pos; buf[len] = '\0';
            }
            editor_redraw(path, buf, len, pos);
            continue;
        }
        if (key == ASTER_KEY_LEFT) { if (pos > 0) --pos; editor_redraw(path, buf, len, pos); continue; }
        if (key == ASTER_KEY_RIGHT) { if (pos < len) ++pos; editor_redraw(path, buf, len, pos); continue; }
        if (key == ASTER_KEY_UP) {
            usize line_start = pos, col;
            while (line_start > 0 && buf[line_start - 1] != '\n') --line_start;
            col = pos - line_start;
            if (line_start > 0) {
                usize prev_end = line_start - 1, prev_start = prev_end, prev_len;
                while (prev_start > 0 && buf[prev_start - 1] != '\n') --prev_start;
                prev_len = prev_end - prev_start;
                pos = prev_start + (col < prev_len ? col : prev_len);
            }
            editor_redraw(path, buf, len, pos); continue;
        }
        if (key == ASTER_KEY_DOWN) {
            usize line_start = pos, col, line_end;
            while (line_start > 0 && buf[line_start - 1] != '\n') --line_start;
            col = pos - line_start;
            line_end = pos;
            while (line_end < len && buf[line_end] != '\n') ++line_end;
            if (line_end < len) {
                usize next_start = line_end + 1, next_end = next_start, next_len;
                while (next_end < len && buf[next_end] != '\n') ++next_end;
                next_len = next_end - next_start;
                pos = next_start + (col < next_len ? col : next_len);
            }
            editor_redraw(path, buf, len, pos); continue;
        }
        if (key == '\n' || (key >= 32 && key <= 126)) {
            if (len < 4096) {
                usize j;
                for (j = len; j > pos; --j) buf[j] = buf[j - 1];
                buf[pos] = (char)key; ++len; buf[len] = '\0'; ++pos;
                editor_redraw(path, buf, len, pos);
            }
        }
    }
}
