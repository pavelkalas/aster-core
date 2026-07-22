/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 *
 * File manager — grafický průzkumník souborového systému.
 */

#include "fm.h"
#include "bootlog.h"
#include "display.h"
#include "drivers.h"
#include "editor.h"
#include "fs_utils.h"

#include "printk.h"
#include "shell.h"
#include "statusbar.h"
#include "storage.h"
#include "string.h"
#include "timer.h"

#define FM_MAX_ENTRIES 64
#define FM_VIEW_ROWS 18

static char g_fm_names[FM_MAX_ENTRIES][64];
static u8 g_fm_types[FM_MAX_ENTRIES];
static u16 g_fm_sizes[FM_MAX_ENTRIES];
static int g_fm_count = 0;

/**
 * Zpětné volání pro sběr položek adresáře – ukládá název, typ a velikost.
 *
 * @param name   Název položky (const char *)
 * @param is_dir 1 pro adresář, 0 pro soubor (u8)
 * @param size   Velikost v bajtech (u16)
 */
static void fm_collect_cb(const char *name, u8 is_dir, u16 size) {
    usize n;
    if (g_fm_count >= FM_MAX_ENTRIES) return;
    n = aster_strlen(name);
    if (n >= 64) n = 63;
    aster_memcpy(g_fm_names[g_fm_count], name, n);
    g_fm_names[g_fm_count][n] = '\0';
    g_fm_types[g_fm_count] = is_dir;
    g_fm_sizes[g_fm_count] = size;
    ++g_fm_count;
}

/**
 * Načte obsah adresáře do interních polí.
 *
 * @param path Cesta k adresáři (const char *)
 */
static void fm_load_dir(const char *path) {
    int i;
    g_fm_count = 0;
    for (i = 0; i < FM_MAX_ENTRIES; ++i) {
        g_fm_names[i][0] = '\0';
        g_fm_types[i] = 0;
        g_fm_sizes[i] = 0;
    }
    asterfs_list_dir(path, fm_collect_cb);
}

/**
 * Zobrazí náhled souboru (prvních 4096 bajtů).
 *
 * @param path Cesta k souboru (const char *)
 */
static void fm_preview_file(const char *path) {
    u8 buf[4096];
    int n = asterfs_read_file(path, buf, 4096);
    display_clear();
    display_set_color(0x07, 0x01);
    printk(" FILE PREVIEW: %s ", path);
    display_set_color(0x0E, 0x08);
    aster_print("\n\n");
    if (n < 0) print_error("Soubor nelze otevrit");
    else { buf[n] = '\0'; printk("%s\n", (char *)buf); }
    display_set_color(0x0E, 0x08);
    aster_print("\n\nStiskni libovolnou klavesu...\n");
    (void)keyboard_read_key();
}

/**
 * Vykreslí obrazovku file manageru – seznam souborů a adresářů.
 *
 * @param cwd      Aktuální adresář (const char *)
 * @param selected Index vybrané položky (int)
 * @param top      Index první zobrazené položky (pro scrollování) (int)
 */
static void fm_draw(const char *cwd, int selected, int top) {
    int i;
    display_set_color(0x0E, 0);
    display_clear();
    display_set_color(0x07, 0);
    printk(" File manager | path: %s ", cwd);
    display_set_color(0x0E, 0);
    aster_print("\n");
    aster_print(" Keymap: U/DOWN=scroll, ENTER=open, E=edit, D=delete, BACKSPACE=up, Q=quit \n\n");

    for (i = 0; i < FM_VIEW_ROWS; ++i) {
        int idx = top + i;
        if (idx >= g_fm_count) break;
        if (idx == selected) display_set_color(0x0F, 0x01);
        else display_set_color(0x0E, 0x08);
        aster_print(idx == selected ? "> " : "  ");
        if (g_fm_types[idx]) printk("[DIR ] %s\n", g_fm_names[idx]);
        else printk("[FILE] %s (%u B)\n", g_fm_names[idx], (unsigned int)g_fm_sizes[idx]);
    }
    display_set_color(0x0E, 0x08);
    render_shell_statusbar();
}

/**
 * Nastaví text v marquee na základě vybrané položky v aktuálním adresáři.
 *
 * @param cwd      Aktuální adresář (const char *)
 * @param selected Index vybrané položky (int)
 */
static void fm_set_selection_marquee(const char *cwd, int selected) {
    char text[120];
    usize p = 0;
    if (g_fm_count <= 0) {
        p = append_text(text, p, sizeof(text), "Prazdna slozka: ");
        p = append_text(text, p, sizeof(text), cwd);
        text[p] = '\0';
        status_set_marquee(text); return;
    }
    if (selected < 0) selected = 0;
    if (selected >= g_fm_count) selected = g_fm_count - 1;

    if (g_fm_types[selected]) {
        p = append_text(text, p, sizeof(text), "Vybrana slozka: ");
        p = append_text(text, p, sizeof(text), g_fm_names[selected]);
        p = append_text(text, p, sizeof(text), "/");
    } else {
        p = append_text(text, p, sizeof(text), "Vybrany soubor: ");
        p = append_text(text, p, sizeof(text), g_fm_names[selected]);
    }
    text[p] = '\0';
    status_set_marquee(text);
}

extern char g_cwd[64];

/**
 * Spustí interaktivní file manager.
 * Umožňuje procházení adresářů, otevírání souborů (ENTER),
 * editaci (E), mazání (D) a návrat zpět (BACKSPACE).
 */
void run_file_manager(void) {
    char fm_cwd[64], last_cwd[64];
    int selected = 0, last_selected = -1, top = 0;
    unsigned long last_anim = timer_ticks();
    unsigned long tick_counter = 0;
    unsigned long step_ticks = 1;
    unsigned long shift_interval = 10;

    aster_memset(fm_cwd, 0, sizeof(fm_cwd));
    aster_memcpy(fm_cwd, g_cwd, aster_strlen(g_cwd));
    aster_memset(last_cwd, 0, sizeof(last_cwd));
    display_set_color(0x0E, 0x08);
    display_clear();
    g_status_marquee_only = 1;

    for (;;) {
        int key;
        unsigned long now;
        fm_load_dir(fm_cwd);
        if (g_fm_count == 0) { selected = 0; top = 0; }
        else {
            if (selected >= g_fm_count) selected = g_fm_count - 1;
            if (selected < 0) selected = 0;
            if (top > selected) top = selected;
            if (selected >= top + FM_VIEW_ROWS) top = selected - FM_VIEW_ROWS + 1;
        }
        if (last_selected != selected || aster_strcmp(last_cwd, fm_cwd) != 0) {
            fm_set_selection_marquee(fm_cwd, selected);
            aster_memset(last_cwd, 0, sizeof(last_cwd));
            aster_memcpy(last_cwd, fm_cwd, aster_strlen(fm_cwd));
            last_selected = selected;
        }
        fm_draw(fm_cwd, selected, top);

        for (;;) {
            unsigned long delta;
            key = keyboard_try_read_key();
            now = timer_ticks();
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
            if (key != -1) break;
            __asm__ volatile ("pause");
        }

        if (key == 'q' || key == 27) break;
        if (key == ASTER_KEY_UP) {
            if (selected > 0) { --selected; if (selected < top) --top; }
            continue;
        }
        if (key == ASTER_KEY_DOWN) {
            if (selected + 1 < g_fm_count) { ++selected; if (selected >= top + FM_VIEW_ROWS) ++top; }
            continue;
        }
        if (key == '\b') {
            char full[64];
            resolve_path_from(fm_cwd, "..", full, sizeof(full));
            aster_memset(fm_cwd, 0, sizeof(fm_cwd));
            aster_memcpy(fm_cwd, full, aster_strlen(full));
            selected = 0; top = 0; continue;
        }
        if (key == '\n' && g_fm_count > 0) {
            char full[64];
            resolve_path_from(fm_cwd, g_fm_names[selected], full, sizeof(full));
            if (g_fm_types[selected]) {
                aster_memset(fm_cwd, 0, sizeof(fm_cwd));
                aster_memcpy(fm_cwd, full, aster_strlen(full));
                selected = 0; top = 0;
            } else fm_preview_file(full);
            continue;
        }
        if ((key == 'e' || key == 'E') && g_fm_count > 0) {
            char full[64];
            resolve_path_from(fm_cwd, g_fm_names[selected], full, sizeof(full));
            if (!g_fm_types[selected]) shell_edit_file(full);
            continue;
        }
        if ((key == 'd' || key == 'D') && g_fm_count > 0) {
            char full[64];
            int confirm;
            resolve_path_from(fm_cwd, g_fm_names[selected], full, sizeof(full));
            display_set_color(0x0F, 0x01);
            aster_print("\nSmazat vybranou polozku? [Y/N]: ");
            display_set_color(0x0E, 0x08);
            confirm = keyboard_read_key(); display_putc('\n');
            if (confirm == 'Y' || confirm == 'y') {
                if (g_fm_types[selected]) { if (fs_remove_tree_abs(full) != 0) print_error("Delete slozky selhal"); }
                else { if (asterfs_remove_file(full) != 0) print_error("Delete souboru selhal"); }
            }
            continue;
        }
    }

    aster_memset(g_cwd, 0, sizeof(g_cwd));
    aster_memcpy(g_cwd, fm_cwd, aster_strlen(fm_cwd));
    g_status_marquee_only = 0;
    status_clear_marquee();
    display_set_color(0x0F, 0x00);
    display_clear();
}
