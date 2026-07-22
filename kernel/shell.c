/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 *
 * Shell — hlavní interaktivní smyčka, parsování příkazů a dispatcher.
 */

#include "shell.h"
#include "auth.h"
#include "bootlog.h"
#include "display.h"
#include "drivers.h"
#include "editor.h"
#include "fm.h"
#include "fs_utils.h"
#include "int.h"
#include "io_ports.h"
#include "memory.h"
#include "printk.h"
#include "process.h"
#include "scheduler.h"
#include "serial.h"
#include "statusbar.h"
#include "storage.h"
#include "string.h"
#include "sysapps_runtime.h"
#include "timer.h"
#include "types.h"

#define ALIASES_FILE "/.aliases"
#define SCREEN_W 80
#define SCREEN_H 25

char g_cwd[64] = "/";
unsigned long g_timer_hz = 100UL;

extern const sysapp_entry_t g_sysapps[];
int run_sysapp_by_name(const char *name);
void print_error(const char *msg);
void show_aster_banner(const char *subtitle);
void print_exec_profile(const void *entry_addr, unsigned long start_ticks);

static const char g_help_lines[][80] = {
    "help [stranka]        - napoveda po strankach",
    "helpall               - interaktivni help Enter/Q",
    "info                  - informace o jadru",
    "memory|mem            - stav pameti",
    "process|ps            - seznam procesu",
    "clear|cls             - vycistit obrazovku",
    "ls                    - vypis aktualni slozky",
    "cd filename           - zmena slozky",
    "makdir filename       - vytvorit slozku",
    "remdir filename       - smazat slozku",
    "movdir src dst [-rec] - presun slozky",
    "copdir src dst [-rec] - kopie slozky",
    "makfile filename      - vytvorit soubor",
    "remfile filename      - smazat soubor",
    "movfile src dst       - presun souboru",
    "copfile src dst       - kopie souboru",
    "cat|read filename     - obsah souboru",
    "write filename text   - zapis textu",
    "install               - instalace systemu na disk",
    "fm                    - file manager",
    "edit filename         - editor (Ctrl+S, ESC)",
    "calc A op B           - vypocet (+ - * /)",
    "tetris                - testovaci hra nad app API",
    "<sysapps nazvy>       - appka ze slozky sysapps",
    "echo text             - vypis textu",
    "ticks                 - pocet tiknuti casovace",
    "alloc N               - alokovat N bajtu",
    "useradd user [pass]   - vytvori uzivatele",
    "passwdch [user]       - zmeni heslo uzivatele",
    "exit                  - odhlaseni do loginu",
    "reboot                - restart systemu",
    "shutdown              - zastavit system",
};

/**
 * Vypíše chybovou hlášku červeně.
 *
 * @param msg Text chyby (const char *)
 */
void print_error(const char *msg) {
    display_set_color(0x0C, 0x00);
    printk("%s\n", msg);
    display_set_color(0x0F, 0x00);
}

/**
 * Zobrazí banner AsterOS s volitelným podtitulem.
 *
 * @param subtitle Podtitul (const char *)
 */
void show_aster_banner(const char *subtitle) {
    display_set_color(0x08, 0x00);
    aster_print("[aster-core v0.13] Copyright (c) 2026 Pavel Kalas\n\n");
    display_set_color(0x0F, 0x00);
    if (subtitle && subtitle[0]) printk("%s\n", subtitle);
}

/**
 * Vypíše profilovací informaci – počet tiků a milisekund.
 *
 * @param entry_addr  Adresa vstupního bodu (const void *)
 * @param delta_ticks Počet tiků (unsigned long)
 */
void print_exec_profile_ticks(const void *entry_addr, unsigned long delta_ticks) {
    unsigned long long scaled = 0;
    unsigned int ms_int = 0, ms_frac7 = 0;
    char frac[8];
    int i;
    if (g_timer_hz != 0) {
        scaled = ((unsigned long long)delta_ticks * 10000000000ULL) / (unsigned long long)g_timer_hz;
        ms_int = (unsigned int)(scaled / 10000000ULL);
        ms_frac7 = (unsigned int)(scaled % 10000000ULL);
    }
    frac[7] = '\0';
    for (i = 6; i >= 0; --i) { frac[i] = (char)('0' + (ms_frac7 % 10U)); ms_frac7 /= 10U; }
    printk("%u.%sms - %p\n", ms_int, frac, entry_addr);
}

/**
 * Vypíše profilovací informaci o době běhu příkazu.
 *
 * @param entry_addr  Adresa vstupního bodu (const void *)
 * @param start_ticks Čas zahájení v ticích (unsigned long)
 */
void print_exec_profile(const void *entry_addr, unsigned long start_ticks) {
    unsigned long end_ticks = timer_ticks();
    unsigned long delta_ticks = end_ticks - start_ticks;
    print_exec_profile_ticks(entry_addr, delta_ticks);
}

/**
 * Najde a spustí sysapp podle jména.
 *
 * @param name Název aplikace (const char *)
 * @return     0 při úspěchu, -1 pokud nenalezena (int)
 */
int run_sysapp_by_name(const char *name) {
    usize i;
    if (!name || name[0] == '\0') return -1;
    for (i = 0; g_sysapps[i].name; ++i) {
        if (aster_strcmp(g_sysapps[i].name, name) == 0) {
            g_sysapps[i].entry();
            render_shell_statusbar();
            return 0;
        }
    }
    return -1;
}

/**
 * Provede restart systému přes I/O porty (0x64, 0xCF9, ACPI porty).
 */
static void cmd_reboot(void) {
    system_shutdown_prepare("restart");
    boot_step_begin("Resetovani hardwaru");
    __asm__ volatile ("cli");
    if (kbc_wait_input_clear()) outb(0x64, 0xFE);
    outb(0xCF9, 0x02); outb(0xCF9, 0x06);
    for (;;) __asm__ volatile ("hlt");
}

/**
 * Provede vypnutí systému přes ACPI a virtuální I/O porty (QEMU, Bochs, VirtualBox).
 */
static void cmd_shutdown_(void) {
    system_shutdown_prepare("vypnuti");
    boot_step_begin("Vypnuti hardwaru");
    __asm__ volatile ("cli");
    outw(0x604, 0x2000); outw(0xB004, 0x2000);
    outw(0x4004, 0x3400); outw(0x0604, 0x2000);
    for (;;) __asm__ volatile ("hlt");
}

/**
 * Vypíše seznam všech procesů z procesové tabulky.
 */
static void shell_processes(void) {
    process_t *table = process_table();
    usize count = process_count();
    usize i;
    if (count == 0) { aster_print("Zadne procesy\n"); return; }
    for (i = 0; i < count; ++i) {
        const process_t *p = &table[i];
        const char *state = "UNK";
        if (p->state == PROCESS_READY) state = "READY";
        if (p->state == PROCESS_RUNNING) state = "RUN";
        if (p->state == PROCESS_BLOCKED) state = "BLOCK";
        if (p->state == PROCESS_EXITED) state = "EXIT";
        printk("pid=%d state=%s prio=%d name=%s\n", (int)p->pid, state, (int)p->priority, p->name ? p->name : "-");
    }
}

/**
 * Zjistí, zda se řetězec cmd shoduje s názvem aliasu dané délky.
 *
 * @param start Začátek názvu aliasu (const char *)
 * @param len   Délka názvu (usize)
 * @param cmd   Porovnávaný příkaz (const char *)
 * @return      1 pokud se shoduje, jinak 0 (int)
 */
static int alias_name_equals(const char *start, usize len, const char *cmd) {
    usize i = 0;
    while (cmd[i] && i < len) { if (start[i] != cmd[i]) return 0; ++i; }
    return i == len && cmd[i] == '\0';
}

/**
 * Vyhledá alias pro daný příkaz v souboru "/.aliases".
 *
 * @param cmd      Příkaz k vyhledání (const char *)
 * @param out      Buffer pro výsledek aliasu (char *)
 * @param out_size Velikost bufferu (usize)
 * @return         1 pokud alias existuje a byl naplněn, jinak 0 (int)
 */
static int alias_lookup(const char *cmd, char *out, usize out_size) {
    char buf[4096];
    int n = asterfs_read_file(ALIASES_FILE, (u8 *)buf, 4096);
    usize i = 0;
    if (!cmd || !out || out_size == 0 || n < 0) return 0;
    buf[n] = '\0';
    while (i < (usize)n) {
        usize line_start = i, name_len = 0, val_start = 0, val_end = 0, p;
        while (i < (usize)n && buf[i] != '\n') ++i;
        p = line_start;
        while (p < (usize)n && (buf[p] == ' ' || buf[p] == '\t')) ++p;
        if (p < (usize)n && buf[p] != '#' && buf[p] != '\n') {
            usize name_start = p;
            while (p < (usize)n && buf[p] != '=' && buf[p] != '\n') ++p;
            if (p < (usize)n && buf[p] == '=') {
                name_len = p - name_start;
                val_start = p + 1; val_end = i;
                while (name_len > 0 && (buf[name_start + name_len - 1] == ' ' || buf[name_start + name_len - 1] == '\t')) --name_len;
                while (val_start < val_end && (buf[val_start] == ' ' || buf[val_start] == '\t')) ++val_start;
                while (val_end > val_start && (buf[val_end - 1] == ' ' || buf[val_end - 1] == '\t' || buf[val_end - 1] == '\r')) --val_end;
                if (name_len > 0 && val_end > val_start && alias_name_equals(&buf[name_start], name_len, cmd)) {
                    usize out_len = val_end - val_start;
                    if (out_len + 1 > out_size) out_len = out_size - 1;
                    aster_memcpy(out, &buf[val_start], out_len);
                    out[out_len] = '\0';
                    return 1;
                }
            }
        }
        if (i < (usize)n && buf[i] == '\n') ++i;
    }
    return 0;
}

/**
 * Zajistí existenci souboru s aliasy. Pokud neexistuje, vytvoří ho
 * s výchozí sadou aliasů.
 */
void ensure_aliases_file(void) {
    static const char defaults[] =
        "ver=info\nmem=memory\nps=process\ncls=clear\ndir=ls\n"
        "md=makdir\nrd=remdir\ntouch=makfile\ndel=remfile\n"
        "copy=copfile\nmove=movfile\nread=cat\ntype=cat\nhalt=shutdown\n";
    if (asterfs_get_type(ALIASES_FILE) >= 0) return;
    if (asterfs_create_file(ALIASES_FILE) != 0) return;
    (void)asterfs_write_file(ALIASES_FILE, (const u8 *)defaults, (u16)(sizeof(defaults) - 1));
}

/**
 * Zjistí, zda je znak bílý.
 *
 * @param c Znak (char)
 * @return  1 pokud je bílý, jinak 0 (int)
 */
static int is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

/**
 * Odstraní bílé znaky z obou konců řetězce (in-place).
 *
 * @param s Řetězec k oříznutí (char *)
 */
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
 *
 * @param cursor Dvouúrovňový ukazatel na aktuální pozici v řetězci (char **)
 * @return       Ukazatel na token, nebo NULL pokud došel text (char *)
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

static void cmd_setup_install(void);

/**
 * Zpětné volání pro výpis položek adresáře (příkaz ls).
 *
 * @param name   Název (const char *)
 * @param is_dir 1 pro adresář, 0 pro soubor (u8)
 * @param size   Velikost (u16)
 */
static void fs_list_cb(const char *name, u8 is_dir, u16 size) {
    printk("%s  %s  %u\n", is_dir ? "DIR " : "FILE", name, (unsigned int)size);
}

/**
 * Spustí jednoduchý "C-like script" – načte soubor a hledá v něm
 * volání printf("text"), které interpretuje a vypíše.
 *
 * @param path Cesta k souboru (const char *)
 */
static void run_c_like_script(const char *path) {
    char src[4096];
    int n = asterfs_read_file(path, (u8 *)src, 4096);
    unsigned long t0 = timer_ticks();
    int i;
    int printed = 0;
    if (n < 0) { print_error("Script nenalezen"); print_exec_profile((const void *)run_c_like_script, t0); return; }
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
    print_exec_profile((const void *)run_c_like_script, t0);
}

#include "serial.h"

/**
 * Provede instalaci systému na disk – vytvoří adresářovou strukturu,
 * uživatele, základní soubory (README, motd, profile, aliases).
 */
static void cmd_setup_install(void) {
    static const char readme[] =
        "aster-core base install\n--------------------\n"
        "System byl nainstalovan prikazem setup.\n"
        "Pro napovedu pouzij: help\n"
        "Aliasy upravis v: /.aliases pomoci edit /.aliases\n";
    static const char motd[] = "Welcome to aster-core v0.13\nType 'help' for commands.\n";
    static const char profile[] = "echo Welcome in aster-core\n";
    static const char installed[] = "installed=1\n";
    char setup_user[AUTH_NAME_LEN], setup_pass[AUTH_PASS_LEN], user_home[64];
    int k;
    usize p = 0;

    aster_print("Setup: instalace systemu na AsterFS disk...\n");
    for (;;) {
        aster_print("Setup uzivatel: ");
        auth_readline_plain(setup_user, sizeof(setup_user));
        if (setup_user[0] == '\0') { print_error("Uzivatel nesmi byt prazdny"); continue; }
        break;
    }
    aster_print("Setup heslo (prazdne = auto-login): ");
    auth_readline_secret(setup_pass, sizeof(setup_pass));

    if (fs_ensure_dir("/etc") != 0 || fs_ensure_dir("/home") != 0 ||
        fs_ensure_dir("/bin") != 0 || fs_ensure_dir("/var") != 0 || fs_ensure_dir("/tmp") != 0) {
        print_error("Setup error: nelze pripravit adresarovou strukturu"); return;
    }

    if (setup_user[0]) {
        p = append_text(user_home, 0, sizeof(user_home), "/home/");
        p = append_text(user_home, p, sizeof(user_home), setup_user);
        user_home[p] = '\0';
        if (fs_ensure_dir(user_home) != 0) { printk("Setup error: home %s\n", user_home); return; }
    }

    if (fs_ensure_file_text("/README.txt", readme) != 0 ||
        fs_ensure_file_text("/etc/motd", motd) != 0 ||
        fs_ensure_file_text("/etc/profile", profile) != 0 ||
        fs_ensure_file_text("/.installed", installed) != 0) {
        print_error("Setup error: nelze zapsat systemove soubory"); return;
    }

    auth_clear_users();
    if (auth_add_user(setup_user, setup_pass) != 0 || auth_save_users() != 0) {
        print_error("Setup error: nelze ulozit uzivatele"); return;
    }
    aster_memset(g_current_user, 0, sizeof(g_current_user));
    p = aster_strlen(setup_user);
    if (p >= sizeof(g_current_user)) p = sizeof(g_current_user) - 1;
    aster_memcpy(g_current_user, setup_user, p);
    ensure_aliases_file();

    aster_print("Setup complete: system nainstalovan na disk\n");
    aster_print("Restartovat system nyni? [y/N] ");
    k = keyboard_read_key();
    if (k != '\n') display_putc((char)k);
    display_putc('\n');
    if (k == 'y' || k == 'Y') {
        if (asterfs_sync() != 0) {
            bootlog_error("Nelze odpojit instalacni medium, stiskni ENTER pro pokracovani");
            for (;;) { int kk = keyboard_read_key(); if (kk == '\n') break; }
        }
        timer_sleep_ms(1500);
        cmd_reboot();
    }
}

/**
 * Hlavní smyčka shellu – čte příkazy, parsuje je a volá příslušné handler funkce.
 */
void shell_loop(void) {
    char line[128];
    char full[64];
    char aliased_cmd[32];
    char *cursor, *cmd, *arg1, *arg2;
    const char *exec_cmd;

    for (;;) {
        render_shell_statusbar();
        printk("[%s] A:%s> ", g_current_user[0] ? g_current_user : "guest", g_cwd);
        keyboard_readline(line, sizeof(line));
        trim_inplace(line);
        cursor = line;
        cmd = next_token(&cursor);
        if (!cmd) continue;

        exec_cmd = cmd;
        if (alias_lookup(cmd, aliased_cmd, sizeof(aliased_cmd)))
            exec_cmd = aliased_cmd;

        if (aster_strcmp(exec_cmd, "help") == 0) {
            arg1 = next_token(&cursor);
            if (arg1) {
                unsigned int page = (unsigned int)parse_u32(arg1, &(int){0});
                if (page < 1 || page > 3) print_error("Pouziti: help [1..3]");
                else {
                    const unsigned int per_page = 11;
                    unsigned int total = sizeof(g_help_lines) / sizeof(g_help_lines[0]);
                    unsigned int start = (page - 1) * per_page;
                    unsigned int end = start + per_page;
                    if (end > total) end = total;
                    unsigned int i;
                    printk("Help stranka %u/%u\n", page, (total + per_page - 1) / per_page);
                    for (i = start; i < end; ++i) printk("  %s\n", g_help_lines[i]);
                }
            } else {
                unsigned int i, total = sizeof(g_help_lines) / sizeof(g_help_lines[0]);
                printk("Help stranka 1/%u\n", (total + 10) / 11);
                for (i = 0; i < 11 && i < total; ++i) printk("  %s\n", g_help_lines[i]);
            }
        } else if (aster_strcmp(exec_cmd, "helpall") == 0) {
            unsigned int i, total = sizeof(g_help_lines) / sizeof(g_help_lines[0]);
            aster_print("helpall: Enter = dalsi radek, Q = konec\n");
            for (i = 0; i < total; ++i) {
                printk("  %s\n", g_help_lines[i]);
                for (;;) { int k = keyboard_read_key(); if (k == '\n') break; if (k == 'q' || k == 'Q' || k == 27) { aster_print("helpall ukoncen\n"); goto help_done; } }
            }
            help_done:;
        } else if (aster_strcmp(exec_cmd, "info") == 0) {
            printk("aster-core v0.13 | Autor: Pavel Kalas | Rok: 2026\ntick=%u\n", (unsigned int)timer_ticks());
        } else if (aster_strcmp(exec_cmd, "memory") == 0) {
            printk("total pages=%u free pages=%u\n", (unsigned int)memory_total_pages(), (unsigned int)memory_free_pages());
        } else if (aster_strcmp(exec_cmd, "process") == 0) shell_processes();
        else if (aster_strcmp(exec_cmd, "clear") == 0) display_clear();
        else if (aster_strcmp(exec_cmd, "ticks") == 0) printk("ticks=%u\n", (unsigned int)timer_ticks());
        else if (aster_strcmp(exec_cmd, "echo") == 0) { while (*cursor && is_space(*cursor)) ++cursor; printk("%s\n", cursor); }
        else if (aster_strcmp(exec_cmd, "ls") == 0) asterfs_list_dir(g_cwd, fs_list_cb);
        else if (aster_strcmp(exec_cmd, "cd") == 0) {
            arg1 = next_token(&cursor);
            if (!arg1) print_error("Pouziti: cd <filename>");
            else {
                resolve_path(arg1, full, sizeof(full));
                if (asterfs_get_type(full) == 1) { aster_memset(g_cwd, 0, sizeof(g_cwd)); aster_memcpy(g_cwd, full, aster_strlen(full)); }
                else print_error("Slozka neexistuje");
            }
        } else if (aster_strcmp(exec_cmd, "makdir") == 0) {
            arg1 = next_token(&cursor);
            if (!arg1) print_error("Pouziti: makdir <filename>");
            else { resolve_path(arg1, full, sizeof(full)); aster_print(asterfs_create_dir(full) == 0 ? "OK\n" : "ERROR\n"); }
        } else if (aster_strcmp(exec_cmd, "remdir") == 0) {
            arg1 = next_token(&cursor);
            if (!arg1) print_error("Pouziti: remdir <filename>");
            else {
                resolve_path(arg1, full, sizeof(full));
                if (aster_strcmp(full, "/") == 0) print_error("Nelze smazat root /");
                else if (asterfs_get_type(full) != 1) print_error("Neexistuje");
                else if (asterfs_remove_dir(full) == 0 || fs_remove_tree_abs(full) == 0) aster_print("OK\n");
                else print_error("Chyba");
            }
        } else if (aster_strcmp(exec_cmd, "copdir") == 0 || aster_strcmp(exec_cmd, "movdir") == 0) {
            int rec = 0, is_move = (aster_strcmp(exec_cmd, "movdir") == 0);
            char src[64], dst[64], *arg3; int rc;
            arg1 = next_token(&cursor); arg2 = next_token(&cursor); arg3 = next_token(&cursor);
            if (!arg1 || !arg2) { print_error("Pouziti: copdir|movdir <src> <dst> [-rec]"); continue; }
            if (arg3) { if (aster_strcmp(arg3, "-rec") == 0) rec = 1; else { print_error("Neznamy prepinac"); continue; } }
            resolve_path(arg1, src, sizeof(src)); resolve_path(arg2, dst, sizeof(dst));
            rc = fs_copy_dir_abs(src, dst, rec);
            if (rc == -2) { print_error("Slozka neni prazdna. Pouzij -rec"); continue; }
            if (rc != 0) { print_error("Chyba kopie"); continue; }
            if (is_move) { if (fs_remove_tree_abs(src) != 0) print_error("Chyba remove"); }
            aster_print("OK\n");
        } else if (aster_strcmp(exec_cmd, "remfile") == 0) {
            arg1 = next_token(&cursor);
            if (!arg1) print_error("Pouziti: remfile <filename>");
            else { resolve_path(arg1, full, sizeof(full)); aster_print(asterfs_remove_file(full) == 0 ? "OK\n" : "ERROR\n"); }
        } else if (aster_strcmp(exec_cmd, "copfile") == 0 || aster_strcmp(exec_cmd, "movfile") == 0) {
            int is_move = (aster_strcmp(exec_cmd, "movfile") == 0);
            char src[64], dst[64];
            arg1 = next_token(&cursor); arg2 = next_token(&cursor);
            if (!arg1 || !arg2) { print_error("Pouziti: copfile|movfile <src> <dst>"); continue; }
            resolve_path(arg1, src, sizeof(src)); resolve_path(arg2, dst, sizeof(dst));
            if (asterfs_get_type(dst) == 1) {
                const char *base = path_basename(src);
                usize dlen = aster_strlen(dst), blen = aster_strlen(base);
                if (dlen + 1 + blen >= sizeof(dst)) { print_error("Cesta prilis dlouha"); continue; }
                dst[dlen] = '/'; aster_memcpy(dst + dlen + 1, base, blen); dst[dlen + 1 + blen] = '\0';
            }
            if (fs_copy_file_abs(src, dst) != 0) { print_error("Chyba kopie"); continue; }
            if (is_move && asterfs_remove_file(src) != 0) { print_error("Chyba remove"); continue; }
            aster_print("OK\n");
        } else if (aster_strcmp(exec_cmd, "cat") == 0) {
            u8 buf[4096]; int n;
            arg1 = next_token(&cursor);
            if (!arg1) print_error("Pouziti: read <filename>");
            else {
                resolve_path(arg1, full, sizeof(full));
                n = asterfs_read_file(full, buf, 4096);
                if (n < 0) print_error("Soubor nenalezen");
                else { buf[n] = '\0'; printk("%s\n", (char *)buf); }
            }
        } else if (aster_strcmp(exec_cmd, "write") == 0) {
            int w; arg1 = next_token(&cursor);
            while (*cursor && is_space(*cursor)) ++cursor;
            arg2 = cursor;
            if (!arg1 || !arg2 || *arg2 == '\0') print_error("Pouziti: write <filename> <text>");
            else {
                resolve_path(arg1, full, sizeof(full));
                w = asterfs_write_file(full, (const u8 *)arg2, (u16)aster_strlen(arg2));
                if (w < 0) print_error("Chyba zapis"); else printk("Zapsano %d B\n", w);
            }
        } else if (aster_strcmp(exec_cmd, "install") == 0) {
            if (system_is_installed()) print_error("Neznamy prikaz. Zadej help.");
            else cmd_setup_install();
        } else if (aster_strcmp(exec_cmd, "fm") == 0) run_file_manager();
        else if (aster_strcmp(exec_cmd, "edit") == 0) {
            arg1 = next_token(&cursor);
            if (!arg1) aster_print("Pouziti: edit <filename>\n");
            else { resolve_path(arg1, full, sizeof(full)); shell_edit_file(full); }
        } else if (aster_strcmp(exec_cmd, "alloc") == 0) {
            int ok; unsigned long n; void *p;
            arg1 = next_token(&cursor); n = parse_u32(arg1, &ok);
            if (!ok) print_error("Pouziti: alloc <bajty>");
            else { p = kmalloc((usize)n); printk("alloc -> %p\n", p); }
        } else if (aster_strcmp(exec_cmd, "useradd") == 0) {
            char pass[AUTH_PASS_LEN];
            arg1 = next_token(&cursor); arg2 = next_token(&cursor);
            if (!arg1) { print_error("Pouziti: useradd <user> [pass]"); continue; }
            pass[0] = '\0';
            if (arg2) { usize plen = aster_strlen(arg2); if (plen >= sizeof(pass)) { print_error("Heslo moc dlouhe"); continue; } aster_memcpy(pass, arg2, plen); pass[plen] = '\0'; }
            if (auth_add_user(arg1, pass) != 0 || auth_save_users() != 0) print_error("Chyba");
            else aster_print("OK\n");
        } else if (aster_strcmp(exec_cmd, "passwdch") == 0) {
            char new_pass[AUTH_PASS_LEN];
            arg1 = next_token(&cursor);
            const char *target = arg1 ? arg1 : g_current_user;
            if (!target || target[0] == '\0' || auth_find_user(target) < 0) { print_error("Neexistuje"); continue; }
            aster_print("Nove heslo: "); auth_readline_secret(new_pass, sizeof(new_pass));
            if (auth_set_pass(target, new_pass) != 0 || auth_save_users() != 0) print_error("Chyba");
            else aster_print("OK\n");
        } else if (aster_strcmp(exec_cmd, "exit") == 0) { aster_memset(g_current_user, 0, sizeof(g_current_user)); return; }
        else if (exec_cmd[0] == '.' && exec_cmd[1] == '/' && exec_cmd[2] != '\0') {
            resolve_path(exec_cmd + 2, full, sizeof(full)); run_c_like_script(full);
        } else if (aster_strcmp(exec_cmd, "reboot") == 0) cmd_reboot();
        else if (aster_strcmp(exec_cmd, "shutdown") == 0) cmd_shutdown_();
        else if (run_sysapp_by_name(exec_cmd) == 0) continue;
        else print_error("Neznamy prikaz. Zadej help.");
    }
}
