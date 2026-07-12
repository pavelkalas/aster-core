/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor implementuje zakladni klavesnicovy vstup pres PS/2 porty.
 * Preklada scancode na ASCII znaky a poskytuje funkci pro nacitani radku,
 * kterou pouziva shell pro prijem prikazu od uzivatele.
 */

#include "display.h"
#include "drivers.h"
#include "timer.h"

#define KBD_HISTORY_MAX 32
#define KBD_HISTORY_LINE_MAX 128

static char g_history[KBD_HISTORY_MAX][KBD_HISTORY_LINE_MAX];
static int g_history_count = 0;
static int g_history_head = 0;
static int g_ctrl_down = 0;
static int g_shift_down = 0;
static void (*g_refresh_cb)(void) = 0;
static unsigned long g_refresh_interval_ticks = 0;

static char apply_letter_case(char c) {
    int upper = g_shift_down;

    if (!upper) {
        return c;
    }

    if (c >= 'a' && c <= 'z') {
        return (char)(c - ('a' - 'A'));
    }

    return c;
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static const char keymap[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
};

void keyboard_init(void) {
}

void keyboard_set_refresh_callback(void (*cb)(void), unsigned long interval_ticks) {
    g_refresh_cb = cb;
    g_refresh_interval_ticks = interval_ticks;
}

int keyboard_try_read_key(void) {
    unsigned char sc;
    int out = -1;

    if ((inb(0x64) & 1) == 0) {
        return -1;
    }

    sc = inb(0x60);

    if (sc == 0xE0) {
        unsigned char ext;

        if ((inb(0x64) & 1) == 0) {
            return -1;
        }

        ext = inb(0x60);
        if (ext == 0x1D) {
            g_ctrl_down = 1;
            return -1;
        }

        if (ext == 0x9D) {
            g_ctrl_down = 0;
            return -1;
        }

        if (ext & 0x80) {
            return -1;
        }

        if (ext == 0x48) {
            out = ASTER_KEY_UP;
            return out;
        }

        if (ext == 0x50) {
            out = ASTER_KEY_DOWN;
            return out;
        }

        if (ext == 0x4B) {
            out = ASTER_KEY_LEFT;
            return out;
        }

        if (ext == 0x4D) {
            out = ASTER_KEY_RIGHT;
            return out;
        }

        return -1;
    }

    if (sc == 0x1D) {
        g_ctrl_down = 1;
        return -1;
    }

    if (sc == 0x9D) {
        g_ctrl_down = 0;
        return -1;
    }

    if (sc == 0x2A || sc == 0x36) {
        g_shift_down = 1;
        return -1;
    }

    if (sc == 0xAA || sc == 0xB6) {
        g_shift_down = 0;
        return -1;
    }

    if (sc & 0x80) {
        return -1;
    }

    if (sc == 0x3B) {
        out = ASTER_KEY_F1;
        return out;
    }

    if (sc == 0x3C) {
        out = ASTER_KEY_F2;
        return out;
    }

    if (sc == 0x1F && g_ctrl_down) {
        out = 19; /* Ctrl+S */
        return out;
    }

    if (sc < 128 && keymap[sc]) {
        out = apply_letter_case(keymap[sc]);
        return out;
    }

    return -1;
}

int keyboard_read_key(void) {
    for (;;) {
        int k = keyboard_try_read_key();
        if (k != -1) {
            return k;
        }

        __asm__ volatile ("pause");
    }
}

static void clear_current_line(int len) {
    int j;

    for (j = 0; j < len; ++j) {
        display_putc('\b');
    }
}

static void set_line_from_history(char *buffer, int *len, int max_len, const char *src) {
    int j = 0;
    int limit = max_len - 1;

    clear_current_line(*len);
    while (src[j] && j < limit) {
        buffer[j] = src[j];
        display_putc(src[j]);
        ++j;
    }

    buffer[j] = '\0';
    *len = j;
}

static void history_push(const char *line) {
    int i;
    int slot;

    if (!line || !line[0]) {
        return;
    }

    if (g_history_count > 0) {
        int last = (g_history_head + KBD_HISTORY_MAX - 1) % KBD_HISTORY_MAX;
        if (g_history[last][0] != '\0') {
            i = 0;
            while (line[i] && g_history[last][i] && line[i] == g_history[last][i]) {
                ++i;
            }
            if (line[i] == '\0' && g_history[last][i] == '\0') {
                return;
            }
        }
    }

    slot = g_history_head;
    for (i = 0; i < KBD_HISTORY_LINE_MAX - 1 && line[i]; ++i) {
        g_history[slot][i] = line[i];
    }
    g_history[slot][i] = '\0';

    g_history_head = (g_history_head + 1) % KBD_HISTORY_MAX;
    if (g_history_count < KBD_HISTORY_MAX) {
        ++g_history_count;
    }
}

int keyboard_readline(char *buffer, int max_len) {
    int i = 0;
    int history_nav = -1;
    unsigned long last_refresh_tick = timer_ticks();

    if (max_len <= 1) {
        return 0;
    }

    for (;;) {
        int c = keyboard_try_read_key();

        if (c == -1) {
            if (g_refresh_cb && g_refresh_interval_ticks > 0) {
                unsigned long now = timer_ticks();
                if ((now - last_refresh_tick) >= g_refresh_interval_ticks) {
                    g_refresh_cb();
                    last_refresh_tick = now;
                }
            }

            __asm__ volatile ("pause");
            continue;
        }

        if (c == ASTER_KEY_F1 || c == ASTER_KEY_F2) {
            continue;
        }

        if (c == ASTER_KEY_UP) {
            if (g_history_count == 0) {
                continue;
            }

            if (history_nav < g_history_count - 1) {
                ++history_nav;
            }

            {
                int idx = (g_history_head + KBD_HISTORY_MAX - 1 - history_nav) % KBD_HISTORY_MAX;
                set_line_from_history(buffer, &i, max_len, g_history[idx]);
            }

            continue;
        }

        if (c == ASTER_KEY_DOWN) {
            if (g_history_count == 0) {
                continue;
            }

            if (history_nav > 0) {
                --history_nav;
                {
                    int idx = (g_history_head + KBD_HISTORY_MAX - 1 - history_nav) % KBD_HISTORY_MAX;
                    set_line_from_history(buffer, &i, max_len, g_history[idx]);
                }
            } else if (history_nav == 0) {
                history_nav = -1;
                clear_current_line(i);
                i = 0;
                buffer[0] = '\0';
            }

            continue;
        }

        if (c == '\n') {
            display_putc('\n');
            buffer[i] = '\0';
            history_push(buffer);
            return i;
        }

        if (c == '\b') {
            if (i > 0) {
                --i;
                display_putc('\b');
            }
            continue;
        }

        if (i < max_len - 1) {
            buffer[i++] = (char)c;
            display_putc((char)c);
            buffer[i] = '\0';
            history_nav = -1;
        }
    }
}
