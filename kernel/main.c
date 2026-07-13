/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Tento soubor je hlavni orchestrator startu celeho jadra.
 * Postupne inicializuje CPU, pamet, procesy, ovladace a preruseni,
 * vypisuje boot stav, zobrazi banner a preda rizeni shell smycce.
 */

#include "cpu.h"
#include "bootlog.h"
#include "display.h"
#include "drivers.h"
#include "memory.h"
#include "printk.h"
#include "process.h"
#include "scheduler.h"
#include "storage.h"
#include "string.h"
#include "sysapps_runtime.h"
#include "syscall.h"
#include "timer.h"

void app_tetris_main(void);
extern const sysapp_entry_t g_sysapps[];

static char g_cwd[ASTERFS_NAME_LEN] = "/";
static unsigned long g_timer_hz = 100UL;

#define FS_TMP_MAX ASTERFS_MAX_FILES
#define FM_MAX_ENTRIES ASTERFS_MAX_FILES
#define FM_VIEW_ROWS 18
#define SCREEN_W 80
#define SCREEN_H 25
#define AUTH_USERS_FILE "/.users"
#define ALIASES_FILE "/.aliases"
#define INSTALL_FLAG_FILE "/.installed"
#define AUTH_MAX_USERS 16
#define AUTH_NAME_LEN 32
#define AUTH_PASS_LEN 32

static char g_fs_tmp_paths[FS_TMP_MAX][ASTERFS_NAME_LEN];
static u8 g_fs_tmp_types[FS_TMP_MAX];
static int g_fs_tmp_count = 0;

typedef struct {
    char name[AUTH_NAME_LEN];
    char pass[AUTH_PASS_LEN];
} auth_user_t;

static auth_user_t g_users[AUTH_MAX_USERS];
static int g_user_count = 0;
static char g_current_user[AUTH_NAME_LEN] = "";

static void trim_inplace(char *s);
static void resolve_path_from(const char *base, const char *name, char *out, usize out_size);
static const char *path_basename(const char *path);
static int fs_remove_tree_abs(const char *root);
static void shell_edit_file(const char *path);
static int auth_save_users(void);
static void auth_readline_plain(char *out, int max_len);
static void auth_readline_secret(char *out, int max_len);
static void auth_clear_users(void);
static int auth_add_user(const char *name, const char *pass);
static void cmd_reboot(void);
static int installer_detach_medium(void);
static int run_sysapp_by_name(const char *name);
static char g_fm_names[FM_MAX_ENTRIES][ASTERFS_NAME_LEN];
static u8 g_fm_types[FM_MAX_ENTRIES];
static u16 g_fm_sizes[FM_MAX_ENTRIES];
static int g_fm_count = 0;
static char g_status_marquee[160];
static int g_status_marquee_on = 0;
static usize g_status_marquee_offset = 0;
static int g_status_marquee_only = 0;
static char g_status_left_hint[80];
static int g_status_left_hint_on = 0;
static int g_boot_splash_active = 0;
static unsigned int g_boot_splash_steps_total = 0;
static unsigned int g_boot_splash_steps_done = 0;
static char g_boot_splash_log[4][56];
static unsigned int g_boot_splash_log_count = 0;

static usize append_text(char *dst, usize pos, usize max, const char *src) {
    while (*src && pos + 1 < max) {
        dst[pos++] = *src++;
    }
    if (max > 0) {
        dst[pos < max ? pos : max - 1] = '\0';
    }
    return pos;
}

static usize append_uint(char *dst, usize pos, usize max, unsigned int v) {
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

static void status_set_marquee(const char *text) {
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

    /* Gap so looped marquee does not stick to itself. */
    for (i = 0; i < 10 && p + 1 < sizeof(g_status_marquee); ++i) {
        g_status_marquee[p++] = ' ';
    }

    g_status_marquee[p] = '\0';
    g_status_marquee_on = (p > 0) ? 1 : 0;
    g_status_marquee_offset = 0;
}

static void status_clear_marquee(void) {
    g_status_marquee[0] = '\0';
    g_status_marquee_on = 0;
    g_status_marquee_offset = 0;
}

static void status_set_left_hint(const char *text) {
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

static void status_clear_left_hint(void) {
    g_status_left_hint[0] = '\0';
    g_status_left_hint_on = 0;
}

static void render_shell_statusbar(void) {
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

    display_fill_row(SCREEN_H - 1, ' ', 0x0E, 0x08);

    if (!g_status_marquee_only) {
        usize p = 0;
        p = append_text(left, p, sizeof(left), "pamet: pages=");
        p = append_uint(left, p, sizeof(left), (unsigned int)total);
        p = append_text(left, p, sizeof(left), " volne=");
        p = append_uint(left, p, sizeof(left), (unsigned int)freep);
        p = append_text(left, p, sizeof(left), " pouzite=");
        p = append_uint(left, p, sizeof(left), (unsigned int)used);
        (void)p;
        display_write_at(SCREEN_H - 1, 0, left, 0x0E, 0x08);
    } else if (g_status_left_hint_on) {
        display_write_at(SCREEN_H - 1, 0, g_status_left_hint, 0x0E, 0x08);
        llen = 0;
        while (g_status_left_hint[llen]) {
            ++llen;
        }
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
        while (g_status_marquee[src_len]) {
            ++src_len;
        }
        if (view_len + 1 > sizeof(middle)) {
            view_len = sizeof(middle) - 1;
        }
        if (src_len > 0) {
            s = g_status_marquee_offset % src_len;
            for (i = 0; i < view_len; ++i) {
                middle[i] = g_status_marquee[(s + i) % src_len];
            }
            middle[view_len] = '\0';
        }
    }

    mlen = 0;
    while (middle[mlen]) ++mlen;
    if (mlen > 0) {
        middle_start = (SCREEN_W > mlen) ? ((SCREEN_W - mlen) / 2) : 0;
        if (g_status_marquee_only) {
            if (middle_start < llen + 6) {
                middle_start = llen + 6;
            }
        } else if (middle_start < llen + 1) {
            middle_start = llen + 1;
        }
        if (middle_start + mlen + 1 < SCREEN_W) {
            display_write_at(SCREEN_H - 1, middle_start, middle, 0x0E, 0x08);
        }
    }

    if (!g_status_marquee_only) {
        right_start = (rlen < SCREEN_W) ? (SCREEN_W - rlen) : 0;
        display_write_at(SCREEN_H - 1, right_start, right, 0x0E, 0x08);
    }

    if (save_row >= SCREEN_H - 1) {
        save_row = SCREEN_H - 2;
    }
    display_set_cursor(save_row, save_col);
}

static void shell_status_refresh_callback(void) {
    render_shell_statusbar();
}

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outw(u16 port, u16 value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static int kbc_wait_input_clear(void) {
    unsigned int i;
    for (i = 0; i < 100000U; ++i) {
        u8 s;
        __asm__ volatile ("inb %1, %0" : "=a"(s) : "Nd"((u16)0x64));
        if ((s & 0x02U) == 0U) {
            return 1;
        }
    }
    return 0;
}

static void print_error(const char *msg) {
    display_set_color(0x0C, 0x00);
    printk("%s\n", msg);
    display_set_color(0x0F, 0x00);
}

static int str_starts_with(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static void boot_splash_render_log(void) {
    unsigned int i;

    display_write_at(13, 20, "Step log (array=4):", 0x0F, 0x00);
    for (i = 0; i < 4U; ++i) {
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

    if (g_boot_splash_log_count < 4U) {
        i = g_boot_splash_log_count;
        ++g_boot_splash_log_count;
    } else {
        for (i = 1; i < 4U; ++i) {
            aster_memcpy(g_boot_splash_log[i - 1], g_boot_splash_log[i], sizeof(g_boot_splash_log[0]));
        }
        i = 3U;
    }

    while (text[n] && n + 1 < sizeof(g_boot_splash_log[0])) {
        g_boot_splash_log[i][n] = text[n];
        ++n;
    }
    g_boot_splash_log[i][n] = '\0';

    boot_splash_render_log();
}

static void boot_splash_begin(unsigned int total_steps) {
    const usize bar_w = 34;
    usize i;
    char bar[36];

    if (total_steps == 0) {
        total_steps = 1;
    }

    g_boot_splash_active = 1;
    g_boot_splash_steps_total = total_steps;
    g_boot_splash_steps_done = 0;
    g_boot_splash_log_count = 0;
    aster_memset(g_boot_splash_log, 0, sizeof(g_boot_splash_log));

    display_set_color(0x0F, 0x00);
    display_clear();

    display_set_color(0x0F, 0x01);
    display_write_at(6, 18, "+------------------------------------------+", 0x0F, 0x01);
    display_write_at(7, 18, "|               ASTEROS CORE               |", 0x0F, 0x01);
    display_write_at(8, 18, "|          booting user environment        |", 0x0F, 0x01);
    display_write_at(9, 18, "+------------------------------------------+", 0x0F, 0x01);
    display_set_color(0x0F, 0x00);

    display_write_at(12, 20, "Loading modules...", 0x0F, 0x00);

    for (i = 0; i < bar_w; ++i) {
        bar[i] = '-';
    }
    bar[bar_w] = '\0';

    display_write_at(18, 20, "[", 0x0E, 0x00);
    display_write_at(18, 21, bar, 0x0A, 0x00);
    display_write_at(18, 21 + bar_w, "]", 0x0E, 0x00);
    display_write_at(19, 34, "0%", 0x0F, 0x00);

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

    p = 0;
    p = append_text(log_line, p, sizeof(log_line), "[");
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

    p = 0;
    p = append_uint(pct, p, sizeof(pct), percent);
    if (p + 1 < sizeof(pct)) {
        pct[p++] = '%';
        pct[p] = '\0';
    }

    display_write_at(19, 34, "    ", 0x0F, 0x00);
    display_write_at(19, 34, pct, 0x0F, 0x00);
}

static void boot_splash_end(void) {
    g_boot_splash_active = 0;
}

static void boot_step_begin(const char *label) {
    if (g_boot_splash_active) {
        char log_line[56];
        usize p = 0;
        p = append_text(log_line, p, sizeof(log_line), "[ ... ] ");
        p = append_text(log_line, p, sizeof(log_line), label ? label : "unknown");
        log_line[p] = '\0';
        boot_splash_push_log(log_line);
        return;
    }
    display_set_color(0x0F, 0x00);
    printk("[ ... ] %s", label);
}

static void boot_step_finish(const char *label, const char *state, u8 state_color) {
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

static void boot_step_ok(const char *label) {
    boot_step_finish(label, "Uspesne", 0x0A);
}

static void boot_step_skip(const char *label) {
    boot_step_finish(label, "SKIP", 0x0E);
}

static int alias_name_equals(const char *start, usize len, const char *cmd) {
    usize i = 0;
    while (cmd[i] && i < len) {
        if (start[i] != cmd[i]) {
            return 0;
        }
        ++i;
    }
    return i == len && cmd[i] == '\0';
}

static int alias_lookup(const char *cmd, char *out, usize out_size) {
    char buf[ASTERFS_DATA_LEN + 1];
    int n = asterfs_read_file(ALIASES_FILE, (u8 *)buf, ASTERFS_DATA_LEN);
    usize i = 0;

    if (!cmd || !out || out_size == 0 || n < 0) {
        return 0;
    }

    buf[n] = '\0';
    while (i < (usize)n) {
        usize line_start = i;
        usize name_len = 0;
        usize val_start = 0;
        usize val_end = 0;
        usize p = 0;

        while (i < (usize)n && buf[i] != '\n') {
            ++i;
        }

        p = line_start;
        while (p < (usize)n && (buf[p] == ' ' || buf[p] == '\t')) {
            ++p;
        }

        if (p < (usize)n && buf[p] != '#' && buf[p] != '\n') {
            usize name_start = p;
            while (p < (usize)n && buf[p] != '=' && buf[p] != '\n') {
                ++p;
            }

            if (p < (usize)n && buf[p] == '=') {
                name_len = p - name_start;
                val_start = p + 1;
                val_end = i;

                while (name_len > 0 && (buf[name_start + name_len - 1] == ' ' || buf[name_start + name_len - 1] == '\t')) {
                    --name_len;
                }
                while (val_start < val_end && (buf[val_start] == ' ' || buf[val_start] == '\t')) {
                    ++val_start;
                }
                while (val_end > val_start && (buf[val_end - 1] == ' ' || buf[val_end - 1] == '\t' || buf[val_end - 1] == '\r')) {
                    --val_end;
                }

                if (name_len > 0 && val_end > val_start && alias_name_equals(&buf[name_start], name_len, cmd)) {
                    usize out_len = val_end - val_start;
                    if (out_len + 1 > out_size) {
                        out_len = out_size - 1;
                    }
                    aster_memcpy(out, &buf[val_start], out_len);
                    out[out_len] = '\0';
                    return out_len > 0 ? 1 : 0;
                }
            }
        }

        if (i < (usize)n && buf[i] == '\n') {
            ++i;
        }
    }

    return 0;
}

static void ensure_aliases_file(void) {
    static const char defaults[] =
        "ver=info\n"
        "mem=memory\n"
        "ps=process\n"
        "cls=clear\n"
        "dir=ls\n"
        "md=makdir\n"
        "rd=remdir\n"
        "touch=makfile\n"
        "del=remfile\n"
        "copy=copfile\n"
        "move=movfile\n"
        "read=cat\n"
        "type=cat\n"
        "halt=shutdown\n";

    if (asterfs_get_type(ALIASES_FILE) >= 0) {
        return;
    }

    if (asterfs_create_file(ALIASES_FILE) != 0) {
        return;
    }

    (void)asterfs_write_file(ALIASES_FILE, (const u8 *)defaults, (u16)(sizeof(defaults) - 1));
}

static int fs_ensure_dir(const char *path) {
    int t;

    if (!path || path[0] == '\0') {
        return -1;
    }
    t = asterfs_get_type(path);
    if (t == 1) {
        return 0;
    }
    if (t == 0) {
        return -1;
    }
    return asterfs_create_dir(path);
}

static int fs_ensure_file_text(const char *path, const char *text) {
    int t;

    if (!path || !text || path[0] == '\0') {
        return -1;
    }
    t = asterfs_get_type(path);
    if (t == 1) {
        return -1;
    }
    if (t < 0 && asterfs_create_file(path) != 0) {
        return -1;
    }

    return asterfs_write_file(path, (const u8 *)text, (u16)aster_strlen(text)) < 0 ? -1 : 0;
}

static int system_is_installed(void) {
    return asterfs_get_type(INSTALL_FLAG_FILE) == 0;
}

static int run_sysapp_by_name(const char *name) {
    usize i;

    if (!name || name[0] == '\0') {
        return -1;
    }

    for (i = 0; g_sysapps[i].name; ++i) {
        if (aster_strcmp(g_sysapps[i].name, name) == 0) {
            g_sysapps[i].entry();
            render_shell_statusbar();
            return 0;
        }
    }

    return -1;
}

static int installer_detach_medium(void) {
    if (asterfs_sync() != 0) {
        return -1;
    }

    /* Removable install medium is not controllable in current runtime. */
    return -1;
}

static void cmd_setup_install(void) {
    static const char readme[] =
        "AsterOS base install\n"
        "--------------------\n"
        "System byl nainstalovan prikazem setup.\n"
        "Pro napovedu pouzij: help\n"
        "Aliasy upravis v: /.aliases\n";
    static const char motd[] =
        "Welcome to AsterOS core v0.1\n"
        "Type 'help' for commands.\n";
    static const char profile[] =
        "echo Welcome in AsterOS\n";
    static const char installed[] =
        "installed=1\n";
    char setup_user[AUTH_NAME_LEN];
    char setup_pass[AUTH_PASS_LEN];
    char user_home[ASTERFS_NAME_LEN];
    int k;
    usize p = 0;

    aster_print("Setup: instalace systemu na AsterFS disk...\n");

    for (;;) {
        aster_print("Setup uzivatel: ");
        auth_readline_plain(setup_user, sizeof(setup_user));
        if (setup_user[0] == '\0') {
            print_error("Uzivatel nesmi byt prazdny");
            continue;
        }
        break;
    }

    aster_print("Setup heslo (prazdne = auto-login): ");
    auth_readline_secret(setup_pass, sizeof(setup_pass));

    if (fs_ensure_dir("/etc") != 0) {
        print_error("Setup error: nelze pripravit slozku /etc");
        return;
    }
    if (fs_ensure_dir("/home") != 0) {
        print_error("Setup error: nelze pripravit slozku /home");
        return;
    }
    if (fs_ensure_dir("/bin") != 0) {
        print_error("Setup error: nelze pripravit slozku /bin");
        return;
    }
    if (fs_ensure_dir("/var") != 0) {
        print_error("Setup error: nelze pripravit slozku /var");
        return;
    }
    if (fs_ensure_dir("/tmp") != 0) {
        print_error("Setup error: nelze pripravit slozku /tmp");
        return;
    }

    if (setup_user[0]) {
        p = 0;
        p = append_text(user_home, p, sizeof(user_home), "/home/");
        p = append_text(user_home, p, sizeof(user_home), setup_user);
        user_home[p] = '\0';

        if (fs_ensure_dir(user_home) != 0) {
            printk("Setup error: nelze pripravit home %s\n", user_home);
            return;
        }
    }

    if (fs_ensure_file_text("/README.txt", readme) != 0) {
        print_error("Setup error: README");
        return;
    }

    if (fs_ensure_file_text("/etc/motd", motd) != 0) {
        print_error("Setup error: /etc/motd");
        return;
    }

    if (fs_ensure_file_text("/etc/profile", profile) != 0) {
        print_error("Setup error: /etc/profile");
        return;
    }

    if (fs_ensure_file_text(INSTALL_FLAG_FILE, installed) != 0) {
        print_error("Setup error: /.installed");
        return;
    }

    auth_clear_users();
    if (auth_add_user(setup_user, setup_pass) != 0 || auth_save_users() != 0) {
        print_error("Setup error: nelze ulozit uzivatele");
        return;
    }

    aster_memset(g_current_user, 0, sizeof(g_current_user));
    p = aster_strlen(setup_user);
    if (p >= sizeof(g_current_user)) {
        p = sizeof(g_current_user) - 1;
    }
    aster_memcpy(g_current_user, setup_user, p);

    ensure_aliases_file();

    aster_print("Setup complete: system nainstalovan na disk\n");
    aster_print("Restartovat system nyni? [y/N] ");
    k = keyboard_read_key();
    if (k != '\n') {
        display_putc((char)k);
    }
    display_putc('\n');

    if (k == 'y' || k == 'Y') {
        if (installer_detach_medium() != 0) {
            bootlog_error("Nelze odpojit instalacni medium, stiskni ENTER pro pokracovani");
            for (;;) {
                int kk = keyboard_read_key();
                if (kk == '\n') {
                    break;
                }
            }
        }
        
        timer_sleep_ms(1500);
        cmd_reboot();
    }
}

static void show_welcome_banner(void) {
    display_set_color(0x0F, 0x01);
    aster_print("\n");
    aster_print("========================================\n");
    aster_print("            ASTEROS KERNEL              \n");
    aster_print("           Autor: Pavel Kalas           \n");
    aster_print("                Rok: 2026               \n");
    aster_print("========================================\n");
    aster_print("\n");
    display_set_color(0x0F, 0x00);
}

static void show_aster_banner(const char *subtitle) {
    display_set_color(0x0F, 0x01);
    aster_print("+------------------------------------------------+");
    aster_print("\n");
    aster_print("|                 AsterOS core v0.1              |");
    aster_print("\n");
    aster_print("|                author: Pavel Kalas             |");
    aster_print("\n");
    aster_print("|               text mode shell system           |");
    aster_print("\n");
    aster_print("+------------------------------------------------+");
    aster_print("\n");
    display_set_color(0x0F, 0x00);

    if (subtitle && subtitle[0]) {
        printk("%s\n", subtitle);
    }
}

static void print_exec_profile_ticks(const void *entry_addr, unsigned long delta_ticks) {
    unsigned long long scaled = 0;
    unsigned int ms_int = 0;
    unsigned int ms_frac7 = 0;
    char frac[8];
    int i;

    if (g_timer_hz != 0) {
        scaled = ((unsigned long long)delta_ticks * 10000000000ULL) / (unsigned long long)g_timer_hz;
        ms_int = (unsigned int)(scaled / 10000000ULL);
        ms_frac7 = (unsigned int)(scaled % 10000000ULL);
    }

    frac[7] = '\0';
    for (i = 6; i >= 0; --i) {
        frac[i] = (char)('0' + (ms_frac7 % 10U));
        ms_frac7 /= 10U;
    }

    printk("%u.%sms - %p\n", ms_int, frac, entry_addr);
}

static void print_exec_profile(const void *entry_addr, unsigned long start_ticks) {
    unsigned long end_ticks = timer_ticks();
    unsigned long delta_ticks = end_ticks - start_ticks;
    print_exec_profile_ticks(entry_addr, delta_ticks);
}

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
    "setup                 - instalace systemu na disk",
    "asrun filename        - spustit AsterScript",
    "fm                    - file manager",
    "./filename            - C-like script printf",
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
    "shutdown|halt         - zastavit system",
    "aliasy                - uprav v /.aliases"
};

static void shell_help_page(unsigned int page) {
    const unsigned int per_page = 11;
    const unsigned int total = (unsigned int)(sizeof(g_help_lines) / sizeof(g_help_lines[0]));
    const unsigned int pages = (total + per_page - 1U) / per_page;
    unsigned int start;
    unsigned int end;
    unsigned int i;

    if (page < 1U || page > pages) {
        printk("Neplatna stranka. Pouzij 1..%u\n", pages);
        return;
    }

    start = (page - 1U) * per_page;
    end = start + per_page;
    if (end > total) {
        end = total;
    }

    printk("Help stranka %u/%u\n", page, pages);
    for (i = start; i < end; ++i) {
        printk("  %s\n", g_help_lines[i]);
    }
    aster_print("Tip: pouzij help <cislo_stranky> nebo helpall\n");
}

static void shell_help_all(void) {
    unsigned int i;
    unsigned int total = (unsigned int)(sizeof(g_help_lines) / sizeof(g_help_lines[0]));

    aster_print("helpall: Enter = dalsi radek, Q = konec\n");
    for (i = 0; i < total; ++i) {
        int k;
        printk("  %s\n", g_help_lines[i]);

        for (;;) {
            k = keyboard_read_key();
            if (k == '\n') {
                break;
            }
            if (k == 'q' || k == 'Q' || k == 27) {
                aster_print("helpall ukoncen\n");
                return;
            }
        }
    }

    aster_print("helpall hotovo\n");
}

static void auth_readline_plain(char *out, int max_len) {
    keyboard_readline(out, max_len);
    trim_inplace(out);
}

static void auth_readline_secret(char *out, int max_len) {
    int i = 0;

    if (max_len <= 1) {
        out[0] = '\0';
        return;
    }

    for (;;) {
        int c = keyboard_read_key();

        if (c == '\n') {
            display_putc('\n');
            out[i] = '\0';
            return;
        }

        if (c == '\b') {
            if (i > 0) {
                --i;
            }
            continue;
        }

        if (c >= 32 && c <= 126 && i < max_len - 1) {
            out[i++] = (char)c;
        }
    }
}

static void auth_clear_users(void) {
    int i;

    for (i = 0; i < AUTH_MAX_USERS; ++i) {
        g_users[i].name[0] = '\0';
        g_users[i].pass[0] = '\0';
    }
    g_user_count = 0;
}

static int auth_find_user(const char *name) {
    int i;
    for (i = 0; i < g_user_count; ++i) {
        if (aster_strcmp(g_users[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int auth_add_user(const char *name, const char *pass) {
    usize nlen;
    usize plen;

    if (!name || name[0] == '\0' || g_user_count >= AUTH_MAX_USERS) {
        return -1;
    }

    if (auth_find_user(name) >= 0) {
        return -1;
    }

    nlen = aster_strlen(name);
    plen = pass ? aster_strlen(pass) : 0;
    if (nlen >= AUTH_NAME_LEN || plen >= AUTH_PASS_LEN) {
        return -1;
    }

    aster_memset(g_users[g_user_count].name, 0, AUTH_NAME_LEN);
    aster_memset(g_users[g_user_count].pass, 0, AUTH_PASS_LEN);
    aster_memcpy(g_users[g_user_count].name, name, nlen);
    if (pass) {
        aster_memcpy(g_users[g_user_count].pass, pass, plen);
    }

    ++g_user_count;
    return 0;
}

static int auth_save_users(void) {
    char buf[ASTERFS_DATA_LEN];
    usize pos = 0;
    int i;

    aster_memset(buf, 0, sizeof(buf));

    for (i = 0; i < g_user_count; ++i) {
        usize nlen = aster_strlen(g_users[i].name);
        usize plen = aster_strlen(g_users[i].pass);

        if (pos + nlen + 1 + plen + 1 >= sizeof(buf)) {
            return -1;
        }

        aster_memcpy(buf + pos, g_users[i].name, nlen);
        pos += nlen;
        buf[pos++] = ':';
        aster_memcpy(buf + pos, g_users[i].pass, plen);
        pos += plen;
        buf[pos++] = '\n';
    }

    return asterfs_write_file(AUTH_USERS_FILE, (const u8 *)buf, (u16)pos) >= 0 ? 0 : -1;
}

static int auth_load_users(void) {
    char buf[ASTERFS_DATA_LEN + 1];
    int n;
    usize i = 0;

    auth_clear_users();

    n = asterfs_read_file(AUTH_USERS_FILE, (u8 *)buf, ASTERFS_DATA_LEN);
    if (n < 0) {
        return -1;
    }

    buf[n] = '\0';

    while (i < (usize)n) {
        char name[AUTH_NAME_LEN];
        char pass[AUTH_PASS_LEN];
        usize ni = 0;
        usize pi = 0;

        while (i < (usize)n && (buf[i] == '\n' || buf[i] == '\r')) {
            ++i;
        }
        if (i >= (usize)n) {
            break;
        }

        while (i < (usize)n && buf[i] != ':' && buf[i] != '\n' && ni < AUTH_NAME_LEN - 1) {
            name[ni++] = buf[i++];
        }
        name[ni] = '\0';

        if (i < (usize)n && buf[i] == ':') {
            ++i;
        }

        while (i < (usize)n && buf[i] != '\n' && pi < AUTH_PASS_LEN - 1) {
            pass[pi++] = buf[i++];
        }
        pass[pi] = '\0';

        while (i < (usize)n && buf[i] != '\n') {
            ++i;
        }
        if (i < (usize)n && buf[i] == '\n') {
            ++i;
        }

        if (name[0] != '\0') {
            if (auth_add_user(name, pass) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

static int auth_set_pass(const char *user, const char *pass) {
    int idx = auth_find_user(user);
    usize plen;

    if (idx < 0) {
        return -1;
    }

    plen = pass ? aster_strlen(pass) : 0;
    if (plen >= AUTH_PASS_LEN) {
        return -1;
    }

    aster_memset(g_users[idx].pass, 0, AUTH_PASS_LEN);
    if (pass) {
        aster_memcpy(g_users[idx].pass, pass, plen);
    }
    return 0;
}

static int auth_find_autologin_user(void) {
    int i;
    for (i = 0; i < g_user_count; ++i) {
        if (g_users[i].pass[0] == '\0') {
            return i;
        }
    }
    return -1;
}

static void auth_first_setup_if_needed(void) {
    char name[AUTH_NAME_LEN];
    char pass[AUTH_PASS_LEN];

    if (auth_load_users() == 0 && g_user_count > 0) {
        return;
    }

    display_clear();
    aster_print("=== First Start Setup ===\n");
    aster_print("Vytvor prvniho uzivatele.\n");

    for (;;) {
        aster_print("Uzivatel: ");
        auth_readline_plain(name, sizeof(name));
        if (name[0] == '\0') {
            print_error("Uzivatel nesmi byt prazdny");
            continue;
        }
        break;
    }

    aster_print("Heslo (prazdne = auto-login): ");
    auth_readline_secret(pass, sizeof(pass));

    auth_clear_users();
    if (auth_add_user(name, pass) != 0 || auth_save_users() != 0) {
        print_error("Chyba ulozeni prvniho uzivatele");
    }
}

static void auth_login_screen(void) {
    char name[AUTH_NAME_LEN];
    char pass[AUTH_PASS_LEN];
    int auto_idx;

    if (!system_is_installed()) {
        aster_memset(g_current_user, 0, sizeof(g_current_user));
        aster_memcpy(g_current_user, "guest", 5);
        return;
    }

    for (;;) {
        if (auth_load_users() != 0 || g_user_count == 0) {
            print_error("Neni vytvoren uzivatel. Spust setup.");
            timer_sleep_ms(1000);
            aster_memset(g_current_user, 0, sizeof(g_current_user));
            aster_memcpy(g_current_user, "guest", 5);
            return;
        }

        auto_idx = auth_find_autologin_user();
        if (auto_idx >= 0) {
            aster_memset(g_current_user, 0, sizeof(g_current_user));
            aster_memcpy(g_current_user, g_users[auto_idx].name, aster_strlen(g_users[auto_idx].name));
            return;
        }

        display_clear();
        show_aster_banner("Login");
        aster_print("Uzivatel: ");
        auth_readline_plain(name, sizeof(name));
        aster_print("Heslo: ");
        auth_readline_secret(pass, sizeof(pass));
        timer_sleep_ms(1000);

        {
            int idx = auth_find_user(name);
            if (idx >= 0 && aster_strcmp(g_users[idx].pass, pass) == 0) {
                aster_memset(g_current_user, 0, sizeof(g_current_user));
                aster_memcpy(g_current_user, g_users[idx].name, aster_strlen(g_users[idx].name));
                return;
            }
        }

        print_error("Spatny login nebo heslo, zkus to znovu...");
        timer_sleep_ms(4000);
    }
}

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void trim_inplace(char *s) {
    usize start = 0;
    usize end = aster_strlen(s);
    usize i = 0;

    while (s[start] && is_space(s[start])) {
        ++start;
    }

    while (end > start && is_space(s[end - 1])) {
        --end;
    }

    while (start < end) {
        s[i++] = s[start++];
    }

    s[i] = '\0';
}

static char *next_token(char **cursor) {
    char *p = *cursor;
    char *start;

    while (*p && is_space(*p)) {
        ++p;
    }

    if (*p == '\0') {
        *cursor = p;
        return 0;
    }

    start = p;
    while (*p && !is_space(*p)) {
        ++p;
    }

    if (*p) {
        *p++ = '\0';
    }

    *cursor = p;
    return start;
}

static unsigned long parse_u32(const char *s, int *ok) {
    unsigned long value = 0;
    *ok = 0;

    if (!s || *s == '\0') {
        return 0;
    }

    while (*s) {
        if (*s < '0' || *s > '9') {
            return 0;
        }

        value = value * 10UL + (unsigned long)(*s - '0');
        ++s;
    }

    *ok = 1;
    return value;
}

static long parse_i32(const char *s, int *ok) {
    int sign = 1;
    unsigned long v;

    if (!s || *s == '\0') {
        *ok = 0;
        return 0;
    }

    if (*s == '-') {
        sign = -1;
        ++s;
    }

    v = parse_u32(s, ok);
    if (!*ok) {
        return 0;
    }

    return (long)v * (long)sign;
}

static void resolve_path(const char *name, char *out, usize out_size) {
    usize i = 0;
    usize j = 0;

    if (!name || !out || out_size < 2) {
        return;
    }

    if (name[0] == '/') {
        while (name[i] && i < out_size - 1) {
            out[i] = name[i];
            ++i;
        }
        out[i] = '\0';
        return;
    }

    if (aster_strcmp(name, ".") == 0) {
        resolve_path(g_cwd, out, out_size);
        return;
    }

    if (aster_strcmp(name, "..") == 0) {
        usize n = aster_strlen(g_cwd);
        if (n <= 1) {
            out[0] = '/';
            out[1] = '\0';
            return;
        }

        while (n > 1 && g_cwd[n - 1] != '/') {
            --n;
        }

        if (n > 1) {
            --n;
        }

        for (i = 0; i < n && i < out_size - 1; ++i) {
            out[i] = g_cwd[i];
        }

        if (i == 0) {
            out[i++] = '/';
        }

        out[i] = '\0';
        return;
    }

    if (aster_strcmp(g_cwd, "/") == 0) {
        out[j++] = '/';
    } else {
        while (g_cwd[i] && j < out_size - 1) {
            out[j++] = g_cwd[i++];
        }
        if (j < out_size - 1) {
            out[j++] = '/';
        }
    }

    i = 0;
    while (name[i] && j < out_size - 1) {
        out[j++] = name[i++];
    }

    out[j] = '\0';
}

static void resolve_path_from(const char *base, const char *name, char *out, usize out_size) {
    usize i = 0;
    usize j = 0;

    if (!base || !name || !out || out_size < 2) {
        return;
    }

    if (name[0] == '/') {
        while (name[i] && i < out_size - 1) {
            out[i] = name[i];
            ++i;
        }
        out[i] = '\0';
        return;
    }

    if (aster_strcmp(name, ".") == 0) {
        resolve_path_from("/", base, out, out_size);
        return;
    }

    if (aster_strcmp(name, "..") == 0) {
        usize n = aster_strlen(base);
        if (n <= 1) {
            out[0] = '/';
            out[1] = '\0';
            return;
        }

        while (n > 1 && base[n - 1] != '/') {
            --n;
        }
        if (n > 1) {
            --n;
        }

        for (i = 0; i < n && i < out_size - 1; ++i) {
            out[i] = base[i];
        }
        if (i == 0) {
            out[i++] = '/';
        }
        out[i] = '\0';
        return;
    }

    if (aster_strcmp(base, "/") == 0) {
        out[j++] = '/';
    } else {
        while (base[i] && j < out_size - 1) {
            out[j++] = base[i++];
        }
        if (j < out_size - 1) {
            out[j++] = '/';
        }
    }

    i = 0;
    while (name[i] && j < out_size - 1) {
        out[j++] = name[i++];
    }
    out[j] = '\0';
}

static int str_ieq(const char *a, const char *b) {
    usize i = 0;
    while (a[i] && b[i]) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) {
            return 0;
        }
        ++i;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static void run_aster_script(const char *path) {
    char src[ASTERFS_DATA_LEN + 1];
    char line[128];
    char script_cwd[ASTERFS_NAME_LEN];
    int n;
    usize pos = 0;

    n = asterfs_read_file(path, (u8 *)src, ASTERFS_DATA_LEN);
    if (n < 0) {
        print_error("AsterScript: soubor nenalezen");
        return;
    }
    src[n] = '\0';

    aster_memset(script_cwd, 0, sizeof(script_cwd));
    aster_memcpy(script_cwd, g_cwd, aster_strlen(g_cwd));

    while (pos < (usize)n) {
        usize li = 0;
        char *cursor;
        char *cmd;
        char *arg1;
        char *arg2;

        while (pos < (usize)n && src[pos] != '\n' && li < sizeof(line) - 1) {
            line[li++] = src[pos++];
        }
        if (pos < (usize)n && src[pos] == '\n') {
            ++pos;
        }
        line[li] = '\0';

        trim_inplace(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        cursor = line;
        cmd = next_token(&cursor);
        if (!cmd) {
            continue;
        }

        if (str_ieq(cmd, "print") || str_ieq(cmd, "echo")) {
            while (*cursor && is_space(*cursor)) {
                ++cursor;
            }
            printk("%s\n", cursor);
        } else if (str_ieq(cmd, "read")) {
            char full[ASTERFS_NAME_LEN];
            u8 out[ASTERFS_DATA_LEN + 1];
            int rn;
            arg1 = next_token(&cursor);
            if (!arg1) {
                print_error("AsterScript read: chybi soubor");
                continue;
            }
            resolve_path_from(script_cwd, arg1, full, sizeof(full));
            rn = asterfs_read_file(full, out, ASTERFS_DATA_LEN);
            if (rn < 0) {
                print_error("AsterScript read: soubor nenalezen");
                continue;
            }
            out[rn] = '\0';
            printk("%s\n", (char *)out);
        } else if (str_ieq(cmd, "write")) {
            char full[ASTERFS_NAME_LEN];
            int wn;
            arg1 = next_token(&cursor);
            while (*cursor && is_space(*cursor)) {
                ++cursor;
            }
            if (!arg1 || *cursor == '\0') {
                print_error("AsterScript write: chybi argumenty");
                continue;
            }
            resolve_path_from(script_cwd, arg1, full, sizeof(full));
            wn = asterfs_write_file(full, (const u8 *)cursor, (u16)aster_strlen(cursor));
            if (wn < 0) {
                print_error("AsterScript write: chyba zapisu");
            }
        } else if (str_ieq(cmd, "mkdir")) {
            char full[ASTERFS_NAME_LEN];
            arg1 = next_token(&cursor);
            if (!arg1) {
                continue;
            }
            resolve_path_from(script_cwd, arg1, full, sizeof(full));
            (void)asterfs_create_dir(full);
        } else if (str_ieq(cmd, "mkfile")) {
            char full[ASTERFS_NAME_LEN];
            arg1 = next_token(&cursor);
            if (!arg1) {
                continue;
            }
            resolve_path_from(script_cwd, arg1, full, sizeof(full));
            (void)asterfs_create_file(full);
        } else if (str_ieq(cmd, "rmfile")) {
            char full[ASTERFS_NAME_LEN];
            arg1 = next_token(&cursor);
            if (!arg1) {
                continue;
            }
            resolve_path_from(script_cwd, arg1, full, sizeof(full));
            (void)asterfs_remove_file(full);
        } else if (str_ieq(cmd, "rmdir")) {
            char full[ASTERFS_NAME_LEN];
            arg1 = next_token(&cursor);
            if (!arg1) {
                continue;
            }
            resolve_path_from(script_cwd, arg1, full, sizeof(full));
            (void)asterfs_remove_dir(full);
        } else if (str_ieq(cmd, "cd")) {
            char full[ASTERFS_NAME_LEN];
            arg1 = next_token(&cursor);
            if (!arg1) {
                continue;
            }
            resolve_path_from(script_cwd, arg1, full, sizeof(full));
            if (asterfs_get_type(full) == 1) {
                aster_memset(script_cwd, 0, sizeof(script_cwd));
                aster_memcpy(script_cwd, full, aster_strlen(full));
            }
        } else if (str_ieq(cmd, "sleep")) {
            int ok;
            unsigned long ms;
            arg1 = next_token(&cursor);
            ms = parse_u32(arg1, &ok);
            if (ok) {
                timer_sleep_ms(ms);
            }
        } else if (str_ieq(cmd, "calc")) {
            int oka;
            int okb;
            long a;
            long b;
            long r;
            char op;
            arg1 = next_token(&cursor);
            arg2 = next_token(&cursor);
            {
                char *arg3 = next_token(&cursor);
                if (!arg1 || !arg2 || !arg3 || aster_strlen(arg2) != 1) {
                    continue;
                }
                a = parse_i32(arg1, &oka);
                b = parse_i32(arg3, &okb);
                if (!oka || !okb) {
                    continue;
                }
                op = arg2[0];
                if (op == '+') r = a + b;
                else if (op == '-') r = a - b;
                else if (op == '*') r = a * b;
                else if (op == '/' && b != 0) r = a / b;
                else continue;
                printk("%d\n", (int)r);
            }
        }
    }
}

static void fm_collect_cb(const char *name, u8 is_dir, u16 size) {
    usize n;
    if (g_fm_count >= FM_MAX_ENTRIES) {
        return;
    }

    n = aster_strlen(name);
    if (n >= ASTERFS_NAME_LEN) {
        n = ASTERFS_NAME_LEN - 1;
    }

    aster_memcpy(g_fm_names[g_fm_count], name, n);
    g_fm_names[g_fm_count][n] = '\0';
    g_fm_types[g_fm_count] = is_dir;
    g_fm_sizes[g_fm_count] = size;
    ++g_fm_count;
}

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

static void fm_preview_file(const char *path) {
    u8 buf[ASTERFS_DATA_LEN + 1];
    int n;

    n = asterfs_read_file(path, buf, ASTERFS_DATA_LEN);
    display_clear();
    display_set_color(0x07, 0x01);
    printk(" FILE PREVIEW: %s ", path);
    display_set_color(0x0E, 0x08);
    aster_print("\n\n");

    if (n < 0) {
        print_error("Soubor nelze otevrit");
    } else {
        int i;
        buf[n] = '\0';
        for (i = 0; i < n; ++i) {
            display_putc((char)buf[i]);
        }
    }

    display_set_color(0x0E, 0x08);
    aster_print("\n\nStiskni libovolnou klavesu...\n");
    (void)keyboard_read_key();
}

static void fm_draw(const char *cwd, int selected, int top) {
    int i;

    display_set_color(0x0E, 0x08);
    display_clear();
    display_set_color(0x07, 0x01);
    printk(" ASTER FILE MANAGER | path: %s ", cwd);

    display_set_color(0x0E, 0x08);
    aster_print("\n");
    aster_print(" up/down=scroll, enter=open, e=edit, d=delete, backspace=up, q=quit \n\n");

    for (i = 0; i < FM_VIEW_ROWS; ++i) {
        int idx = top + i;
        if (idx >= g_fm_count) {
            break;
        }

        if (idx == selected) {
            display_set_color(0x0F, 0x01);
            aster_print("> ");
        } else {
            display_set_color(0x0E, 0x08);
            aster_print("  ");
        }

        if (g_fm_types[idx]) {
            printk("[DIR ] %s\n", g_fm_names[idx]);
        } else {
            printk("[FILE] %s (%u B)\n", g_fm_names[idx], (unsigned int)g_fm_sizes[idx]);
        }
    }

    display_set_color(0x0E, 0x08);
    render_shell_statusbar();
}

static void fm_set_selection_marquee(const char *cwd, int selected) {
    char text[120];
    usize p = 0;

    if (g_fm_count <= 0) {
        p = append_text(text, p, sizeof(text), "Prazdna slozka: ");
        p = append_text(text, p, sizeof(text), cwd);
        text[p] = '\0';
        status_set_marquee(text);
        return;
    }

    if (selected < 0) {
        selected = 0;
    }
    if (selected >= g_fm_count) {
        selected = g_fm_count - 1;
    }

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

static void run_file_manager(void) {
    char fm_cwd[ASTERFS_NAME_LEN];
    char last_cwd[ASTERFS_NAME_LEN];
    int selected = 0;
    int last_selected = -1;
    int top = 0;
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
        char full[ASTERFS_NAME_LEN];
        int key;
        unsigned long now;

        fm_load_dir(fm_cwd);

        if (g_fm_count == 0) {
            selected = 0;
            top = 0;
        } else {
            if (selected >= g_fm_count) {
                selected = g_fm_count - 1;
            }
            if (selected < 0) {
                selected = 0;
            }
            if (top > selected) {
                top = selected;
            }
            if (selected >= top + FM_VIEW_ROWS) {
                top = selected - FM_VIEW_ROWS + 1;
            }
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

            // Timer-based posun - každých N tiků
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

            if (key != -1) {
                break;
            }

            __asm__ volatile ("pause");
        }

        if (key == 'q' || key == 27) {
            break;
        }

        if (key == ASTER_KEY_UP) {
            if (selected > 0) {
                --selected;
                if (selected < top) {
                    --top;
                }
            }
            continue;
        }

        if (key == ASTER_KEY_DOWN) {
            if (selected + 1 < g_fm_count) {
                ++selected;
                if (selected >= top + FM_VIEW_ROWS) {
                    ++top;
                }
            }
            continue;
        }

        if (key == '\b') {
            resolve_path_from(fm_cwd, "..", full, sizeof(full));
            aster_memset(fm_cwd, 0, sizeof(fm_cwd));
            aster_memcpy(fm_cwd, full, aster_strlen(full));
            selected = 0;
            top = 0;
            continue;
        }

        if (key == '\n' && g_fm_count > 0) {
            resolve_path_from(fm_cwd, g_fm_names[selected], full, sizeof(full));
            if (g_fm_types[selected]) {
                aster_memset(fm_cwd, 0, sizeof(fm_cwd));
                aster_memcpy(fm_cwd, full, aster_strlen(full));
                selected = 0;
                top = 0;
            } else {
                fm_preview_file(full);
            }
            continue;
        }

        if ((key == 'e' || key == 'E') && g_fm_count > 0) {
            resolve_path_from(fm_cwd, g_fm_names[selected], full, sizeof(full));
            if (!g_fm_types[selected]) {
                shell_edit_file(full);
            }
            continue;
        }

        if ((key == 'd' || key == 'D') && g_fm_count > 0) {
            int confirm;
            resolve_path_from(fm_cwd, g_fm_names[selected], full, sizeof(full));

            display_set_color(0x0F, 0x01);
            aster_print("\nSmazat vybranou polozku? [Y/N]: ");
            display_set_color(0x0E, 0x08);
            confirm = keyboard_read_key();
            display_putc('\n');

            if (confirm == 'Y' || confirm == 'y') {
                if (g_fm_types[selected]) {
                    if (fs_remove_tree_abs(full) != 0) {
                        print_error("Delete slozky selhal");
                    }
                } else {
                    if (asterfs_remove_file(full) != 0) {
                        print_error("Delete souboru selhal");
                    }
                }
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

static const char *path_basename(const char *path) {
    usize i = aster_strlen(path);

    while (i > 0) {
        if (path[i - 1] == '/') {
            return &path[i];
        }
        --i;
    }

    return path;
}

static int path_is_self_or_child(const char *root, const char *path) {
    usize n;

    if (!root || !path) {
        return 0;
    }

    if (aster_strcmp(root, path) == 0) {
        return 1;
    }

    n = aster_strlen(root);
    if (n == 0) {
        return 0;
    }

    if (aster_strncmp(path, root, n) != 0) {
        return 0;
    }

    return path[n] == '/';
}

static int map_path_prefix(const char *src_root, const char *dst_root, const char *path, char *out, usize out_size) {
    usize src_n;
    usize dst_n;
    usize suffix_n;

    if (!path_is_self_or_child(src_root, path)) {
        return -1;
    }

    src_n = aster_strlen(src_root);
    dst_n = aster_strlen(dst_root);

    if (aster_strcmp(src_root, path) == 0) {
        if (dst_n + 1 > out_size) {
            return -1;
        }
        aster_memcpy(out, dst_root, dst_n);
        out[dst_n] = '\0';
        return 0;
    }

    suffix_n = aster_strlen(path + src_n);
    if (dst_n + suffix_n + 1 > out_size) {
        return -1;
    }

    aster_memcpy(out, dst_root, dst_n);
    aster_memcpy(out + dst_n, path + src_n, suffix_n);
    out[dst_n + suffix_n] = '\0';
    return 0;
}

static void fs_collect_cb(const char *name, u8 is_dir, u16 size) {
    usize n;
    (void)size;

    if (g_fs_tmp_count >= FS_TMP_MAX) {
        return;
    }

    n = aster_strlen(name);
    if (n >= ASTERFS_NAME_LEN) {
        n = ASTERFS_NAME_LEN - 1;
    }

    aster_memcpy(g_fs_tmp_paths[g_fs_tmp_count], name, n);
    g_fs_tmp_paths[g_fs_tmp_count][n] = '\0';
    g_fs_tmp_types[g_fs_tmp_count] = is_dir;
    ++g_fs_tmp_count;
}

static void fs_collect_all(void) {
    g_fs_tmp_count = 0;
    asterfs_list(fs_collect_cb);
}

static int fs_dir_has_children(const char *path) {
    int i;

    fs_collect_all();
    for (i = 0; i < g_fs_tmp_count; ++i) {
        if (path_is_self_or_child(path, g_fs_tmp_paths[i]) && aster_strcmp(path, g_fs_tmp_paths[i]) != 0) {
            return 1;
        }
    }

    return 0;
}

static int fs_copy_file_abs(const char *src, const char *dst) {
    u8 buf[ASTERFS_DATA_LEN];
    int n;

    if (asterfs_get_type(src) != 0) {
        return -1;
    }

    n = asterfs_read_file(src, buf, ASTERFS_DATA_LEN);
    if (n < 0) {
        return -1;
    }

    if (asterfs_write_file(dst, buf, (u16)n) < 0) {
        return -1;
    }

    return 0;
}

static int fs_copy_dir_abs(const char *src, const char *dst, int recursive) {
    int i;
    char mapped[ASTERFS_NAME_LEN];

    if (asterfs_get_type(src) != 1) {
        return -1;
    }

    if (asterfs_get_type(dst) >= 0) {
        return -1;
    }

    if (!recursive && fs_dir_has_children(src)) {
        return -2;
    }

    if (asterfs_create_dir(dst) != 0) {
        return -1;
    }

    if (!recursive) {
        return 0;
    }

    fs_collect_all();

    for (i = 0; i < g_fs_tmp_count; ++i) {
        if (!g_fs_tmp_types[i]) {
            continue;
        }
        if (aster_strcmp(g_fs_tmp_paths[i], src) == 0) {
            continue;
        }
        if (!path_is_self_or_child(src, g_fs_tmp_paths[i])) {
            continue;
        }

        if (map_path_prefix(src, dst, g_fs_tmp_paths[i], mapped, sizeof(mapped)) != 0) {
            return -1;
        }

        if (asterfs_create_dir(mapped) != 0) {
            return -1;
        }
    }

    for (i = 0; i < g_fs_tmp_count; ++i) {
        if (g_fs_tmp_types[i]) {
            continue;
        }
        if (!path_is_self_or_child(src, g_fs_tmp_paths[i])) {
            continue;
        }

        if (map_path_prefix(src, dst, g_fs_tmp_paths[i], mapped, sizeof(mapped)) != 0) {
            return -1;
        }

        if (fs_copy_file_abs(g_fs_tmp_paths[i], mapped) != 0) {
            return -1;
        }
    }

    return 0;
}

static int fs_remove_tree_abs(const char *root) {
    int i;
    int j;

    fs_collect_all();

    for (i = 0; i < g_fs_tmp_count; ++i) {
        if (g_fs_tmp_types[i]) {
            continue;
        }
        if (!path_is_self_or_child(root, g_fs_tmp_paths[i])) {
            continue;
        }
        if (asterfs_remove_file(g_fs_tmp_paths[i]) != 0) {
            return -1;
        }
    }

    for (i = 0; i < g_fs_tmp_count; ++i) {
        int best = -1;
        usize best_len = 0;

        for (j = 0; j < g_fs_tmp_count; ++j) {
            usize len;
            if (!g_fs_tmp_types[j]) {
                continue;
            }
            if (g_fs_tmp_paths[j][0] == '\0') {
                continue;
            }
            if (!path_is_self_or_child(root, g_fs_tmp_paths[j])) {
                continue;
            }

            len = aster_strlen(g_fs_tmp_paths[j]);
            if (best == -1 || len > best_len) {
                best = j;
                best_len = len;
            }
        }

        if (best == -1) {
            break;
        }

        if (asterfs_remove_dir(g_fs_tmp_paths[best]) != 0) {
            return -1;
        }

        g_fs_tmp_paths[best][0] = '\0';
    }

    return 0;
}

static void editor_redraw(const char *path, const char *buf, usize len, usize pos) {
    usize i;
    usize line = 1;
    usize cursor_row = 0;
    usize cursor_col = 0;
    int cursor_set = 0;

    for (i = 0; i < pos && i < len; ++i) {
        if (buf[i] == '\n') {
            ++line;
        }
    }

    display_set_color(0x0F, 0x00);
    display_clear();
    display_set_color(0x0F, 0x01);
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

    if (!cursor_set) {
        display_get_cursor(&cursor_row, &cursor_col);
    }

    aster_print("\n\n");
    display_set_color(0x0F, 0x00);
    render_shell_statusbar();
    display_set_cursor(cursor_row, cursor_col);
}

static void shell_edit_file(const char *path) {
    char buf[ASTERFS_DATA_LEN + 1];
    int n = asterfs_read_file(path, (u8 *)buf, ASTERFS_DATA_LEN);
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

    while (prefix[tp] && tp + 1 < sizeof(title)) {
        title[tp] = prefix[tp];
        ++tp;
    }
    while (base[bi] && tp + 1 < sizeof(title)) {
        title[tp++] = base[bi++];
    }
    title[tp] = '\0';

    if (step_ticks == 0) {
        step_ticks = 1;
    }

    if (n < 0) {
        if (asterfs_create_file(path) != 0) {
            aster_print("Editor: nelze vytvorit soubor\n");
            return;
        }
        n = 0;
    }

    len = (usize)n;
    buf[len] = '\0';
    pos = len;
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

        if (key == -1) {
            __asm__ volatile ("pause");
            continue;
        }

        if (key == 27) {
            g_status_marquee_only = 0;
            status_clear_left_hint();
            status_clear_marquee();
            display_clear();
            return;
        }

        if (key == 19) {
            (void)asterfs_write_file(path, (const u8 *)buf, (u16)len);
            display_set_color(0x0A, 0x00);
            aster_print("Saved\n");
            display_set_color(0x0F, 0x00);
            editor_redraw(path, buf, len, pos);
            continue;
        }

        if (key == '\b') {
            if (len > 0 && pos > 0) {
                usize j;
                for (j = pos - 1; j + 1 < len; ++j) {
                    buf[j] = buf[j + 1];
                }
                --len;
                --pos;
                buf[len] = '\0';
            }
            editor_redraw(path, buf, len, pos);
            continue;
        }

        if (key == ASTER_KEY_LEFT) {
            if (pos > 0) {
                --pos;
            }
            editor_redraw(path, buf, len, pos);
            continue;
        }

        if (key == ASTER_KEY_RIGHT) {
            if (pos < len) {
                ++pos;
            }
            editor_redraw(path, buf, len, pos);
            continue;
        }

        if (key == ASTER_KEY_UP) {
            usize line_start = pos;
            usize col;

            while (line_start > 0 && buf[line_start - 1] != '\n') {
                --line_start;
            }

            col = pos - line_start;
            if (line_start > 0) {
                usize prev_end = line_start - 1;
                usize prev_start = prev_end;
                usize prev_len;

                while (prev_start > 0 && buf[prev_start - 1] != '\n') {
                    --prev_start;
                }

                prev_len = prev_end - prev_start;
                pos = prev_start + (col < prev_len ? col : prev_len);
            }

            editor_redraw(path, buf, len, pos);
            continue;
        }

        if (key == ASTER_KEY_DOWN) {
            usize line_start = pos;
            usize col;
            usize line_end;

            while (line_start > 0 && buf[line_start - 1] != '\n') {
                --line_start;
            }

            col = pos - line_start;
            line_end = pos;
            while (line_end < len && buf[line_end] != '\n') {
                ++line_end;
            }

            if (line_end < len) {
                usize next_start = line_end + 1;
                usize next_end = next_start;
                usize next_len;

                while (next_end < len && buf[next_end] != '\n') {
                    ++next_end;
                }

                next_len = next_end - next_start;
                pos = next_start + (col < next_len ? col : next_len);
            }

            editor_redraw(path, buf, len, pos);
            continue;
        }

        if (key == '\n' || (key >= 32 && key <= 126)) {
            if (len < ASTERFS_DATA_LEN) {
                usize j;
                for (j = len; j > pos; --j) {
                    buf[j] = buf[j - 1];
                }
                buf[pos] = (char)key;
                ++len;
                buf[len] = '\0';
                ++pos;
                editor_redraw(path, buf, len, pos);
            }
        }
    }
}

static void fs_list_cb(const char *name, u8 is_dir, u16 size) {
    printk("%s  %s  %u\n", is_dir ? "DIR " : "FILE", name, (unsigned int)size);
}

static void run_c_like_script(const char *path) {
    char src[ASTERFS_DATA_LEN + 1];
    int n = asterfs_read_file(path, (u8 *)src, ASTERFS_DATA_LEN);
    unsigned long t0 = timer_ticks();
    int i;
    int printed = 0;

    if (n < 0) {
        print_error("Script nenalezen");
        print_exec_profile((const void *)run_c_like_script, t0);
        return;
    }

    src[n] = '\0';

    for (i = 0; src[i] != '\0'; ++i) {
        if (str_starts_with(&src[i], "printf(\"")) {
            i += 8;
            while (src[i] != '\0') {
                char c = src[i++];

                if (c == '\\') {
                    char esc = src[i++];
                    if (esc == 'n') display_putc('\n');
                    else if (esc == 't') display_putc('\t');
                    else if (esc == '"') display_putc('"');
                    else if (esc == '\\') display_putc('\\');
                    else display_putc(esc);
                    continue;
                }

                if (c == '"' && src[i] == ')') {
                    if (src[i + 1] == ';') {
                        ++i;
                    }
                    break;
                }

                display_putc(c);
                printed = 1;
            }
        }
    }

    if (printed) {
        display_putc('\n');
    } else {
        print_error("Nepodporovany C kod. Podpora: printf(\"text\");");
    }

    print_exec_profile((const void *)run_c_like_script, t0);
}

static void show_calc_ui(long a, long b, char op, long r) {
    display_clear();
    display_set_color(0x0F, 0x01);
    aster_print("+--------------------------------------+\n");
    aster_print("|          ASTER CALCULATOR            |\n");
    aster_print("+--------------------------------------+\n");
    display_set_color(0x0F, 0x00);
    printk("\n  %d %c %d = %d\n", (int)a, op, (int)b, (int)r);
    aster_print("\nStiskni libovolnou klavesu pro navrat...\n");
    (void)keyboard_read_key();
    display_clear();
}

static void cmd_reboot(void) {
    display_clear();
    display_set_color(0x0F, 0x00);
    aster_print("System restart sequence\n\n");

    boot_step_begin("Stopping user session");
    aster_memset(g_current_user, 0, sizeof(g_current_user));
    boot_step_ok("Stopping user session");

    boot_step_begin("Unloading modules");
    keyboard_set_refresh_callback(0, 0);
    boot_step_ok("Unloading modules");

    boot_step_begin("Freeing memory");
    g_status_marquee_only = 0;
    status_clear_marquee();
    status_clear_left_hint();
    boot_step_ok("Freeing memory");

    boot_step_begin("Syncing filesystems");
    if (asterfs_sync() == 0) {
        boot_step_ok("Syncing filesystems");
    } else {
        boot_step_finish("Syncing filesystems", "CHYBA", 0x0C);
    }

    timer_sleep_ms(2000);

    boot_step_begin("Issuing hardware reset");
    __asm__ volatile ("cli");

    if (kbc_wait_input_clear()) {
        outb(0x64, 0xFE);
    }

    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void cmd_shutdown(void) {
    aster_print("Shutdown...\n");
    __asm__ volatile ("cli");

    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    outw(0x0604, 0x2000);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void print_process_line(const process_t *p) {
    const char *state = "UNK";

    if (p->state == PROCESS_READY) state = "READY";
    if (p->state == PROCESS_RUNNING) state = "RUN";
    if (p->state == PROCESS_BLOCKED) state = "BLOCK";
    if (p->state == PROCESS_EXITED) state = "EXIT";

    printk("pid=%d state=%s prio=%d name=%s\n", (int)p->pid, state, (int)p->priority, p->name ? p->name : "-");
}

static void shell_processes(void) {
    process_t *table = process_table();
    usize count = process_count();
    usize i;

    if (count == 0) {
        aster_print("Zadne procesy\n");
        return;
    }

    for (i = 0; i < count; ++i) {
        print_process_line(&table[i]);
    }
}

static void demo_task_a(void) {
    printk("[taskA] start\n");
}

static void demo_task_b(void) {
    printk("[taskB] start\n");
}

static void shell_loop(void) {
    char line[128];
    char full[ASTERFS_NAME_LEN];
    char aliased_cmd[32];
    char *cursor;
    char *cmd;
    const char *exec_cmd;
    char *arg1;
    char *arg2;

    for (;;) {
        render_shell_statusbar();
        printk("[%s] A:%s> ", g_current_user[0] ? g_current_user : "guest", g_cwd);
        keyboard_readline(line, sizeof(line));
        trim_inplace(line);

        cursor = line;
        cmd = next_token(&cursor);
        if (!cmd) {
            continue;
        }

        exec_cmd = cmd;
        if (alias_lookup(cmd, aliased_cmd, sizeof(aliased_cmd))) {
            exec_cmd = aliased_cmd;
        }

        if (aster_strcmp(exec_cmd, "help") == 0) {
            int ok = 0;
            unsigned int page = 1;
            arg1 = next_token(&cursor);

            if (arg1) {
                page = (unsigned int)parse_u32(arg1, &ok);
                if (!ok || page == 0) {
                    print_error("Pouziti: help [1..N]");
                } else {
                    shell_help_page(page);
                }
            } else {
                shell_help_page(1);
            }
        } else if (aster_strcmp(exec_cmd, "helpall") == 0) {
            shell_help_all();
        } else if (aster_strcmp(exec_cmd, "info") == 0) {
            printk("AsterOS Kernel | Autor: Pavel Kalas | Rok: 2026\n");
            printk("tick=%u\n", (unsigned int)timer_ticks());
        } else if (aster_strcmp(exec_cmd, "memory") == 0) {
            printk("total pages=%u free pages=%u\n", (unsigned int)memory_total_pages(), (unsigned int)memory_free_pages());
        } else if (aster_strcmp(exec_cmd, "process") == 0) {
            shell_processes();
        } else if (aster_strcmp(exec_cmd, "clear") == 0) {
            display_clear();
        } else if (aster_strcmp(exec_cmd, "ticks") == 0) {
            printk("ticks=%u\n", (unsigned int)timer_ticks());
        } else if (aster_strcmp(exec_cmd, "echo") == 0) {
            while (*cursor && is_space(*cursor)) {
                ++cursor;
            }
            printk("%s\n", cursor);
        } else if (aster_strcmp(exec_cmd, "ls") == 0) {
            asterfs_list_dir(g_cwd, fs_list_cb);
        } else if (aster_strcmp(exec_cmd, "cd") == 0) {
            arg1 = next_token(&cursor);
            if (!arg1) {
                print_error("Pouziti: cd <filename>");
            } else {
                resolve_path(arg1, full, sizeof(full));
                if (asterfs_get_type(full) == 1) {
                    aster_memset(g_cwd, 0, sizeof(g_cwd));
                    aster_memcpy(g_cwd, full, aster_strlen(full));
                } else {
                    print_error("Slozka neexistuje");
                }
            }
        } else if (aster_strcmp(exec_cmd, "makdir") == 0) {
            arg1 = next_token(&cursor);
            if (!arg1) {
                print_error("Pouziti: makdir <filename>");
            } else {
                resolve_path(arg1, full, sizeof(full));
                if (asterfs_create_dir(full) == 0) {
                    aster_print("OK\n");
                } else {
                    print_error("Chyba create dir");
                }
            }
        } else if (aster_strcmp(exec_cmd, "remdir") == 0) {
            arg1 = next_token(&cursor);
            if (!arg1) {
                print_error("Pouziti: remdir <filename>");
            } else {
                resolve_path(arg1, full, sizeof(full));
                if (aster_strcmp(full, "/") == 0) {
                    print_error("Nelze smazat root slozku /");
                } else if (asterfs_get_type(full) != 1) {
                    print_error("Slozka neexistuje");
                } else if (asterfs_remove_dir(full) == 0 || fs_remove_tree_abs(full) == 0) {
                    aster_print("OK\n");
                } else {
                    print_error("Chyba remove dir");
                }
            }
        } else if (aster_strcmp(exec_cmd, "copdir") == 0 || aster_strcmp(exec_cmd, "movdir") == 0) {
            int rec = 0;
            int is_move = (aster_strcmp(exec_cmd, "movdir") == 0);
            char src[ASTERFS_NAME_LEN];
            char dst[ASTERFS_NAME_LEN];
            char *arg3;
            int rc;

            arg1 = next_token(&cursor);
            arg2 = next_token(&cursor);
            arg3 = next_token(&cursor);

            if (!arg1 || !arg2) {
                print_error("Pouziti: copdir|movdir <src> <dst> [-rec]");
                continue;
            }

            if (arg3) {
                if (aster_strcmp(arg3, "-rec") == 0) {
                    rec = 1;
                } else {
                    print_error("Neznamy prepinac. Pouzij -rec");
                    continue;
                }
            }

            resolve_path(arg1, src, sizeof(src));
            resolve_path(arg2, dst, sizeof(dst));

            rc = fs_copy_dir_abs(src, dst, rec);
            if (rc == -2) {
                print_error("Slozka neni prazdna. Pouzij -rec");
                continue;
            }
            if (rc != 0) {
                print_error("Chyba kopie slozky");
                continue;
            }

            if (is_move) {
                if (!rec && fs_dir_has_children(src)) {
                    print_error("Move dir: pouzij -rec pro nepradznou slozku");
                    continue;
                }

                if (rec) {
                    if (fs_remove_tree_abs(src) != 0) {
                        print_error("Chyba remove puvodni slozky");
                        continue;
                    }
                } else if (asterfs_remove_dir(src) != 0) {
                    print_error("Chyba remove puvodni slozky");
                    continue;
                }
            }

            aster_print("OK\n");
        } else if (aster_strcmp(exec_cmd, "makfile") == 0) {
            arg1 = next_token(&cursor);
            if (!arg1) {
                print_error("Pouziti: makfile <filename>");
            } else {
                resolve_path(arg1, full, sizeof(full));
                if (asterfs_create_file(full) == 0) {
                    aster_print("OK\n");
                } else {
                    print_error("Chyba create file");
                }
            }
        } else if (aster_strcmp(exec_cmd, "remfile") == 0) {
            arg1 = next_token(&cursor);
            if (!arg1) {
                print_error("Pouziti: remfile <filename>");
            } else {
                resolve_path(arg1, full, sizeof(full));
                if (asterfs_remove_file(full) == 0) {
                    aster_print("OK\n");
                } else {
                    print_error("Chyba remove file");
                }
            }
        } else if (aster_strcmp(exec_cmd, "copfile") == 0 || aster_strcmp(exec_cmd, "movfile") == 0) {
            int is_move = (aster_strcmp(exec_cmd, "movfile") == 0);
            char src[ASTERFS_NAME_LEN];
            char dst[ASTERFS_NAME_LEN];

            arg1 = next_token(&cursor);
            arg2 = next_token(&cursor);
            if (!arg1 || !arg2) {
                print_error("Pouziti: copfile|movfile <src> <dst>");
                continue;
            }

            resolve_path(arg1, src, sizeof(src));
            resolve_path(arg2, dst, sizeof(dst));

            if (asterfs_get_type(dst) == 1) {
                const char *base = path_basename(src);
                usize dlen = aster_strlen(dst);
                usize blen = aster_strlen(base);
                if (dlen + 1 + blen >= sizeof(dst)) {
                    print_error("Cilova cesta je moc dlouha");
                    continue;
                }
                dst[dlen] = '/';
                aster_memcpy(dst + dlen + 1, base, blen);
                dst[dlen + 1 + blen] = '\0';
            }

            if (fs_copy_file_abs(src, dst) != 0) {
                print_error("Chyba kopie souboru");
                continue;
            }

            if (is_move && asterfs_remove_file(src) != 0) {
                print_error("Chyba remove puvodniho souboru");
                continue;
            }

            aster_print("OK\n");
        } else if (aster_strcmp(exec_cmd, "cat") == 0) {
            u8 buf[ASTERFS_DATA_LEN + 1];
            int n;
            unsigned long t0 = timer_ticks();
            arg1 = next_token(&cursor);
            if (!arg1) {
                print_error("Pouziti: read <filename>");
            } else {
                resolve_path(arg1, full, sizeof(full));
                n = asterfs_read_file(full, buf, ASTERFS_DATA_LEN);
                if (n < 0) {
                    print_error("Soubor nenalezen");
                } else {
                    buf[n] = '\0';
                    printk("%s\n", (char *)buf);
                }
            }
            print_exec_profile((const void *)asterfs_read_file, t0);
        } else if (aster_strcmp(exec_cmd, "write") == 0) {
            int w;
            unsigned long t0 = timer_ticks();
            arg1 = next_token(&cursor);
            while (*cursor && is_space(*cursor)) {
                ++cursor;
            }
            arg2 = cursor;

            if (!arg1 || !arg2 || *arg2 == '\0') {
                print_error("Pouziti: write <filename> <text>");
            } else {
                resolve_path(arg1, full, sizeof(full));
                w = asterfs_write_file(full, (const u8 *)arg2, (u16)aster_strlen(arg2));
                if (w < 0) {
                    print_error("Chyba zapis");
                } else {
                    printk("Zapsano %d B\n", w);
                }
            }
            print_exec_profile((const void *)asterfs_write_file, t0);
        } else if (aster_strcmp(exec_cmd, "setup") == 0) {
            if (system_is_installed()) {
                print_error("Neznamy prikaz. Zadej help.");
            } else {
                cmd_setup_install();
            }
        } else if (aster_strcmp(exec_cmd, "asrun") == 0) {
            arg1 = next_token(&cursor);
            if (!arg1) {
                print_error("Pouziti: asrun <filename>");
            } else {
                resolve_path(arg1, full, sizeof(full));
                run_aster_script(full);
            }
        } else if (aster_strcmp(exec_cmd, "fm") == 0) {
            run_file_manager();
        } else if (aster_strcmp(exec_cmd, "edit") == 0) {
            arg1 = next_token(&cursor);
            if (!arg1) {
                aster_print("Pouziti: edit <filename>\n");
            } else {
                resolve_path(arg1, full, sizeof(full));
                shell_edit_file(full);
            }
        } else if (aster_strcmp(exec_cmd, "calc") == 0) {
            int oka;
            int okb;
            long a;
            long b;
            long r = 0;
            char op;
            char *arg3;
            unsigned long t0 = timer_ticks();
            unsigned long calc_delta_ticks = 0;

            arg1 = next_token(&cursor);
            arg2 = next_token(&cursor);
            arg3 = next_token(&cursor);

            if (!arg1 || !arg2 || !arg3 || aster_strlen(arg2) != 1) {
                print_error("Pouziti: calc <A> <op> <B>");
            } else {
                a = parse_i32(arg1, &oka);
                b = parse_i32(arg3, &okb);
                op = arg2[0];

                if (!oka || !okb) {
                    print_error("Chyba: A/B musi byt cisla");
                } else {
                    if (op == '+') r = a + b;
                    else if (op == '-') r = a - b;
                    else if (op == '*') r = a * b;
                    else if (op == '/') {
                        if (b == 0) {
                            print_error("Chyba: deleni nulou");
                            print_exec_profile((const void *)show_calc_ui, t0);
                            continue;
                        }
                        r = a / b;
                    } else {
                        print_error("Operator musi byt + - * /");
                        print_exec_profile((const void *)show_calc_ui, t0);
                        continue;
                    }

                    calc_delta_ticks = timer_ticks() - t0;
                    show_calc_ui(a, b, op, r);
                }
            }

            if (calc_delta_ticks != 0) {
                print_exec_profile_ticks((const void *)show_calc_ui, calc_delta_ticks);
            } else {
                print_exec_profile((const void *)show_calc_ui, t0);
            }
        } else if (aster_strcmp(exec_cmd, "tetris") == 0) {
            app_tetris_main();
            render_shell_statusbar();
        } else if (aster_strcmp(exec_cmd, "alloc") == 0) {
            int ok;
            unsigned long n;
            void *p;
            arg1 = next_token(&cursor);
            n = parse_u32(arg1, &ok);
            if (!ok) {
                print_error("Pouziti: alloc <bajty>");
            } else {
                p = kmalloc((usize)n);
                printk("alloc -> %p\n", p);
            }
        } else if (aster_strcmp(exec_cmd, "useradd") == 0) {
            char pass[AUTH_PASS_LEN];
            arg1 = next_token(&cursor);
            arg2 = next_token(&cursor);
            if (!arg1) {
                print_error("Pouziti: useradd <user> [pass]");
                continue;
            }

            pass[0] = '\0';
            if (arg2) {
                usize plen = aster_strlen(arg2);
                if (plen >= sizeof(pass)) {
                    print_error("Heslo je moc dlouhe");
                    continue;
                }
                aster_memcpy(pass, arg2, plen);
                pass[plen] = '\0';
            }

            if (auth_add_user(arg1, pass) != 0 || auth_save_users() != 0) {
                print_error("Chyba pri vytvareni uzivatele");
            } else {
                aster_print("OK\n");
            }
        } else if (aster_strcmp(exec_cmd, "passwdch") == 0) {
            char new_pass[AUTH_PASS_LEN];
            const char *target;

            arg1 = next_token(&cursor);
            target = arg1 ? arg1 : g_current_user;

            if (!target || target[0] == '\0' || auth_find_user(target) < 0) {
                print_error("Uzivatel neexistuje");
                continue;
            }

            aster_print("Nove heslo (prazdne = bez hesla): ");
            auth_readline_secret(new_pass, sizeof(new_pass));

            if (auth_set_pass(target, new_pass) != 0 || auth_save_users() != 0) {
                print_error("Chyba zmeny hesla");
            } else {
                aster_print("OK\n");
            }
        } else if (aster_strcmp(exec_cmd, "exit") == 0) {
            aster_memset(g_current_user, 0, sizeof(g_current_user));
            return;
        } else if (exec_cmd[0] == '.' && exec_cmd[1] == '/' && exec_cmd[2] != '\0') {
            resolve_path(exec_cmd + 2, full, sizeof(full));
            run_c_like_script(full);
        } else if (aster_strcmp(exec_cmd, "reboot") == 0) {
            cmd_reboot();
        } else if (aster_strcmp(exec_cmd, "shutdown") == 0) {
            cmd_shutdown();
        } else if (run_sysapp_by_name(exec_cmd) == 0) {
            continue;
        } else {
            print_error("Neznamy prikaz. Zadej help.");
        }
    }
}

void kmain(void) {
    display_init();

    boot_step_begin("Serial ovladac");
    serial_init();
    if (serial_is_ready()) {
        boot_step_ok("Serial ovladac");
    } else {
        boot_step_skip("Serial ovladac");
    }

    boot_step_begin("CPU jadro");
    cpu_init();
    boot_step_ok("CPU jadro");

    boot_step_begin("Spravce pameti nacten");
    memory_init();
    boot_step_ok("Spravce pameti nacten");

    boot_step_begin("Spravce procesu");
    process_init();
    boot_step_ok("Spravce procesu");

    boot_step_begin("Planovac");
    scheduler_init();
    boot_step_ok("Planovac");

    boot_step_begin("Vrstva syscalls");
    syscall_init();
    boot_step_ok("Vrstva syscalls");

    boot_step_begin("Klavesnicovy ovladac");
    keyboard_init();
    boot_step_ok("Klavesnicovy ovladac");

    boot_step_begin("Casovac");
    timer_init((unsigned int)g_timer_hz);
    boot_step_ok("Casovac");

    boot_step_begin("Obnova shellu");
    keyboard_set_refresh_callback(shell_status_refresh_callback, g_timer_hz ? g_timer_hz : 100UL);
    boot_step_ok("Obnova shellu");

    boot_step_begin("Diskovy ovladac");
    storage_init();
    boot_step_ok("Diskovy ovladac");

    boot_step_begin("Preruseni");
    interrupts_init();
    boot_step_ok("Preruseni");

    aster_print("\nBoot sekvence dokoncena\n");
    timer_sleep_ms(1500);

    for (;;) {
        auth_login_screen();
        ensure_aliases_file();
        display_clear();
        if (system_is_installed()) {
            show_aster_banner("Shell");
        } else {
            show_aster_banner("Live shell");
            aster_print("Live mode: bez hesla\n");
            aster_print("Pro instalaci na disk spust: setup\n");
        }
        aster_print("Pro zobrazeni prikazu pouzij 'help'\n");
        shell_loop();
    }
}
