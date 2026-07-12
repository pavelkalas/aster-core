/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento ovladac realizuje textovou konzoli nad VGA pameti na adrese 0xB8000.
 * Ridi pozici kurzoru, barvy znaku, zpracovani ridicich znaku
 * a automaticky scroll, kdyz vystup presahne posledni radek obrazovky.
 */

#include "display.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_CONTENT_HEIGHT (VGA_HEIGHT - 1)

static volatile u16 *const vga = (u16 *)0xB8000;
static u8 color = 0x0F;
static usize row = 0;
static usize col = 0;

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void update_hw_cursor(void) {
    u16 pos = (u16)(row * VGA_WIDTH + col);

    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

static inline u16 vga_entry(char c, u8 clr) {
    return (u16)c | ((u16)clr << 8);
}

static void scroll_if_needed(void) {
    usize r;
    usize c;

    if (row < VGA_CONTENT_HEIGHT) {
        return;
    }

    for (r = 1; r < VGA_CONTENT_HEIGHT; ++r) {
        for (c = 0; c < VGA_WIDTH; ++c) {
            vga[(r - 1) * VGA_WIDTH + c] = vga[r * VGA_WIDTH + c];
        }
    }

    for (c = 0; c < VGA_WIDTH; ++c) {
        vga[(VGA_CONTENT_HEIGHT - 1) * VGA_WIDTH + c] = vga_entry(' ', color);
    }

    row = VGA_CONTENT_HEIGHT - 1;
}

void display_set_color(u8 fg, u8 bg) {
    color = (u8)((bg << 4) | (fg & 0x0F));
}

void display_clear(void) {
    usize i;

    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        vga[i] = vga_entry(' ', color);
    }

    row = 0;
    col = 0;
    update_hw_cursor();
}

void display_init(void) {
    display_set_color(0x0F, 0x00);
    display_clear();
}

void display_putc(char c) {
    if (c == '\n') {
        col = 0;
        ++row;
        scroll_if_needed();
        update_hw_cursor();
        return;
    }

    if (c == '\r') {
        col = 0;
        update_hw_cursor();
        return;
    }

    if (c == '\b') {
        if (col > 0) {
            --col;
        } else if (row > 0) {
            --row;
            col = VGA_WIDTH - 1;
        }

        vga[row * VGA_WIDTH + col] = vga_entry(' ', color);
        update_hw_cursor();
        return;
    }

    if (c == '\t') {
        col = (col + 4) & ~3ULL;
        if (col >= VGA_WIDTH) {
            col = 0;
            ++row;
            scroll_if_needed();
        }
        update_hw_cursor();
        return;
    }

    vga[row * VGA_WIDTH + col] = vga_entry(c, color);
    ++col;

    if (col >= VGA_WIDTH) {
        col = 0;
        ++row;
        scroll_if_needed();
    }

    update_hw_cursor();
}

void display_write(const char *text) {
    while (*text) {
        display_putc(*text++);
    }
}

void display_get_cursor(usize *out_row, usize *out_col) {
    if (out_row) {
        *out_row = row;
    }
    if (out_col) {
        *out_col = col;
    }
}

void display_set_cursor(usize new_row, usize new_col) {
    if (new_row >= VGA_HEIGHT) {
        new_row = VGA_HEIGHT - 1;
    }
    if (new_col >= VGA_WIDTH) {
        new_col = VGA_WIDTH - 1;
    }

    row = new_row;
    col = new_col;
    update_hw_cursor();
}

void display_fill_row(usize target_row, char ch, u8 fg, u8 bg) {
    usize c;
    u8 clr;

    if (target_row >= VGA_HEIGHT) {
        return;
    }

    clr = (u8)((bg << 4) | (fg & 0x0F));
    for (c = 0; c < VGA_WIDTH; ++c) {
        vga[target_row * VGA_WIDTH + c] = vga_entry(ch, clr);
    }
}

void display_write_at(usize target_row, usize target_col, const char *text, u8 fg, u8 bg) {
    usize c = target_col;
    u8 clr;

    if (!text || target_row >= VGA_HEIGHT || target_col >= VGA_WIDTH) {
        return;
    }

    clr = (u8)((bg << 4) | (fg & 0x0F));
    while (*text && c < VGA_WIDTH) {
        vga[target_row * VGA_WIDTH + c] = vga_entry(*text++, clr);
        ++c;
    }
}
