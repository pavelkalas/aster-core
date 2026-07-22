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

/**
 * Zapíše bajt na I/O port.
 *
 * @param port  Adresa portu (u16)
 * @param value Hodnota k zápisu (u8)
 */
static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * Aktualizuje hardwarový kurzor VGA (přes CRT kontrolér).
 */
static void update_hw_cursor(void) {
    u16 pos = (u16)(row * VGA_WIDTH + col);

    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

/**
 * Vytvoří VGA entry (znak + barva) pro zápis do bufferu.
 *
 * @param c   Znak (char)
 * @param clr Barva (u8)
 * @return    16bitová hodnota pro VGA buffer (u16)
 */
static inline u16 vga_entry(char c, u8 clr) {
    return (u16)c | ((u16)clr << 8);
}

/**
 * Posune obsah obrazovky o řádek nahoru, pokud je kurzor na posledním řádku.
 */
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

/**
 * Nastaví barvu popředí a pozadí pro textový výstup.
 *
 * @param fg Barva popředí (u8)
 * @param bg Barva pozadí (u8)
 */
void display_set_color(u8 fg, u8 bg) {
    color = (u8)((bg << 4) | (fg & 0x0F));
}

/**
 * Smaže celou obrazovku a nastaví kurzor na pozici (0,0).
 */
void display_clear(void) {
    usize i;

    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        vga[i] = vga_entry(' ', color);
    }

    row = 0;
    col = 0;
    update_hw_cursor();
}

/**
 * Inicializuje display – nastaví výchozí barvu a vyčistí obrazovku.
 */
void display_init(void) {
    display_set_color(0x0F, 0x00);
    display_clear();
}

/**
 * Vypíše jeden znak na obrazovku. Zpracovává řídicí znaky:
 *   \n – nový řádek
 *   \r – návrat vozíku
 *   \b – backspace
 *   \t – tabulátor
 *
 * @param c Znak k vypsání (char)
 */
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

/**
 * Vypíše řetězec na obrazovku.
 *
 * @param text Řetězec (const char *)
 */
void display_write(const char *text) {
    while (*text) {
        display_putc(*text++);
    }
}

/**
 * Získá aktuální pozici kurzoru.
 *
 * @param out_row Ukazatel na řádek (usize *), může být NULL
 * @param out_col Ukazatel na sloupec (usize *), může být NULL
 */
void display_get_cursor(usize *out_row, usize *out_col) {
    if (out_row) {
        *out_row = row;
    }
    if (out_col) {
        *out_col = col;
    }
}

/**
 * Nastaví kurzor na zadanou pozici.
 *
 * @param new_row Řádek (usize)
 * @param new_col Sloupec (usize)
 */
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

/**
 * Vyplní celý řádek obrazovky zadaným znakem a barvou.
 *
 * @param target_row Cílový řádek (usize)
 * @param ch         Znak výplně (char)
 * @param fg         Barva popředí (u8)
 * @param bg         Barva pozadí (u8)
 */
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

/**
 * Zapíše text na zadanou pozici obrazovky s danou barvou.
 *
 * @param target_row   Řádek (usize)
 * @param target_col   Sloupec (usize)
 * @param text         Řetězec (const char *)
 * @param fg           Barva popředí (u8)
 * @param bg           Barva pozadí (u8)
 */
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
