/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Shell — hlavní interaktivní smyčka, parsování příkazů a dispatcher.
 * Všechny příkazy jsou implementovány jako samostatné sysapps.
 */

#include "shell.h"
#include "aster_api.h"
#include "auth.h"
#include "display.h"
#include "fs_utils.h"
#include "keyboard.h"
#include "printk.h"
#include "statusbar.h"
#include "storage.h"
#include "string.h"
#include "sysapps_runtime.h"
#include "timer.h"

#define SCREEN_W 80
#define SCREEN_H 25

char g_cwd[64] = "/";
unsigned long g_timer_hz = 100UL;

extern const sysapp_entry_t g_sysapps[];
int run_sysapp_by_name(const char *name);

void print_error(const char *msg);
void show_aster_banner(const char *subtitle);

/**
 * Vypíše chybovou hlášku červeně.
 */
void print_error(const char *msg) {
    aster_api_set_color(0x0C, 0x00);
    aster_api_print_line(msg);
    aster_api_set_color(0x0F, 0x00);
}

/**
 * Zobrazí banner AsterOS s volitelným podtitulem.
 */
void show_aster_banner(const char *subtitle) {
    aster_api_set_color(0x08, 0x00);
    aster_print("[aster-core v0.13] Copyright (c) 2026 Pavel Kalas\n\n");
    aster_api_set_color(0x0F, 0x00);
    if (subtitle && subtitle[0]) printk("%s\n", subtitle);
}

/**
 * Odstraní bílé znaky z obou konců řetězce (in-place).
 */
static int is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

void trim_inplace(char *s) {
    usize start = 0, end = aster_strlen(s), i = 0;
    while (s[start] && is_space(s[start])) ++start;
    while (end > start && is_space(s[end - 1])) --end;
    while (start < end) s[i++] = s[start++];
    s[i] = '\0';
}

/**
 * Vrátí další token (slovo) z řetězce, oddělený bílými znaky.
 * Mění vstupní řetězec in-place (vkládá null terminátory).
 */
static char *next_token(char **cursor) {
    char *p = *cursor, *start;
    while (*p && is_space(*p)) ++p;
    if (*p == '\0') { *cursor = p; return 0; }
    start = p;
    while (*p && !is_space(*p)) ++p;
    if (*p) *p++ = '\0';
    *cursor = p;
    return start;
}

/**
 * Spustí jednoduchý "C-like script" – načte soubor a hledá v něm
 * volání printf("text"), které interpretuje a vypíše.
 */
static void run_c_like_script(const char *path) {
    char src[4096];
    int n = asterfs_read_file(path, (u8 *)src, 4096);
    int i;
    int printed = 0;

    if (n < 0) {
        print_error("Script nenalezen");
        return;
    }
    src[n] = '\0';
    for (i = 0; src[i] != '\0'; ++i) {
        if (src[i] == 'p' && src[i+1] == 'r' && src[i+2] == 'i' && src[i+3] == 'n' &&
            src[i+4] == 't' && src[i+5] == 'f' && src[i+6] == '(' && src[i+7] == '\"') {
            i += 8;
            while (src[i] != '\0') {
                char c = src[i++];
                if (c == '\\') { char esc = src[i++];
                    if (esc == 'n') display_putc('\n');
                    else if (esc == 't') display_putc('\t');
                    else if (esc == '"') display_putc('"');
                    else if (esc == '\\') display_putc('\\');
                    else display_putc(esc);
                    continue;
                }
                if (c == '"' && src[i] == ')') { if (src[i+1] == ';') ++i; break; }
                display_putc(c); printed = 1;
            }
        }
    }
    if (printed) display_putc('\n');
    else print_error("Nepodporovany C kod. Podpora: printf(\"text\");");
}

/**
 * Najde a spustí sysapp podle jména.
 * Před spuštěním nastaví g_sysapp_argc/g_sysapp_argv z globálních proměnných.
 */
int run_sysapp_by_name(const char *name) {
    usize i;
    if (!name || name[0] == '\0') return -1;
    for (i = 0; g_sysapps[i].name; ++i) {
        if (aster_strcmp(g_sysapps[i].name, name) == 0) {
            /* Obnovení sysapp_argc z globální proměnné (nastaveno shellem) */
            g_sysapps[i].entry();
            render_shell_statusbar();
            return 0;
        }
    }
    return -1;
}

/**
 * Hlavní smyčka shellu – čte příkazy, parsuje je na tokeny,
 * nastaví argumenty a volá příslušné sysapp handler funkce.
 */
void shell_loop(void) {
    char line[128];
    char full[64];
    char *tokens[18];   /* 1 cmd + max 16 args + null */
    char *cursor;
    int tok_count;
    int i;

    for (;;) {
        render_shell_statusbar();
        printk("[%s] A:%s> ", g_current_user[0] ? g_current_user : "guest", g_cwd);
        keyboard_readline(line, sizeof(line));
        trim_inplace(line);
        if (line[0] == '\0') continue;

        /* Tokenizace – rozděl vstup na jednotlivá slova */
        cursor = line;
        tok_count = 0;
        for (i = 0; i < 17; ++i) {
            char *t = next_token(&cursor);
            if (!t) break;
            tokens[i] = t;
            tok_count = i + 1;
        }

        if (tok_count == 0) continue;

        /* Nastav argumenty pro sysapp */
        g_sysapp_argc = tok_count;
        for (i = 0; i < tok_count && i < SYSAPP_MAX_ARGS; ++i) {
            g_sysapp_argv[i] = tokens[i];
        }
        g_shell_should_exit = 0;

        {
            const char *exec_cmd = tokens[0];

            /* Aliasy: touch -> makfile, halt -> shutdown */
            if (aster_strcmp(exec_cmd, "touch") == 0) {
                g_sysapp_argv[0] = "makfile";
                if (run_sysapp_by_name("makfile") == 0) continue;
            }

            if (aster_strcmp(exec_cmd, "halt") == 0) {
                g_sysapp_argv[0] = "shutdown";
                if (run_sysapp_by_name("shutdown") == 0) continue;
            }

            /* ./script – spustit C-like script */
            if (exec_cmd[0] == '.' && exec_cmd[1] == '/' && exec_cmd[2] != '\0') {
                resolve_path(exec_cmd + 2, full, sizeof(full));
                run_c_like_script(full);
                continue;
            }

            /* Standardní dispatch – hledej sysapp podle jména */
            if (run_sysapp_by_name(exec_cmd) == 0) {
                /* Po každém příkazu zkontroluj, zda se nemá ukončit smyčka */
                if (g_shell_should_exit) {
                    aster_memset(g_current_user, 0, sizeof(g_current_user));
                    return;
                }
                continue;
            }

            /* Neznámý příkaz */
            print_error("Neznamy prikaz. Zadej help.");
        }
    }
}
