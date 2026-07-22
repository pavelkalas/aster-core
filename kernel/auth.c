/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 *
 * Auth — uživatelská autentizace, login, správa uživatelů.
 */

#include "auth.h"
#include "bootlog.h"
#include "display.h"
#include "drivers.h"
#include "printk.h"
#include "shell.h"
#include "storage.h"
#include "string.h"
#include "timer.h"
#include "fs_utils.h"

#define AUTH_USERS_FILE "/.users"

static auth_user_t g_users[AUTH_MAX_USERS];
static int g_user_count = 0;

char g_current_user[AUTH_NAME_LEN] = "";
usize g_current_user_size = AUTH_NAME_LEN;

/**
 * Přečte řádek z klávesnice a odstraní bílé znaky z okrajů.
 * Výsledek uloží do `out`.
 *
 * @param out     Cílový buffer (char *)
 * @param max_len Maximální délka (int)
 */
void auth_readline_plain(char *out, int max_len) {
    extern void trim_inplace(char *s);
    keyboard_readline(out, max_len);
    trim_inplace(out);
}

/**
 * Přečte řádek z klávesnice, ale místo zadávaných znaků nic nevypisuje
 * (vhodné pro hesla). Ukončuje se Enterem, Backspace maže znaky.
 *
 * @param out     Cílový buffer (char *)
 * @param max_len Maximální délka (int)
 */
void auth_readline_secret(char *out, int max_len) {
    int i = 0;
    if (max_len <= 1) { out[0] = '\0'; return; }

    for (;;) {
        int c = keyboard_read_key();
        if (c == '\n') { display_putc('\n'); out[i] = '\0'; return; }
        if (c == '\b') { if (i > 0) --i; continue; }
        if (c >= 32 && c <= 126 && i < max_len - 1) out[i++] = (char)c;
    }
}

/**
 * Smaže všechny uživatele z interní tabulky.
 */
void auth_clear_users(void) {
    int i;
    for (i = 0; i < AUTH_MAX_USERS; ++i) {
        g_users[i].name[0] = '\0';
        g_users[i].pass[0] = '\0';
    }
    g_user_count = 0;
}

/**
 * Najde uživatele podle jména v interní tabulce.
 *
 * @param name Hledané uživatelské jméno (const char *)
 * @return     Index v tabulce, nebo -1 pokud nebyl nalezen (int)
 */
int auth_find_user(const char *name) {
    int i;
    for (i = 0; i < g_user_count; ++i)
        if (aster_strcmp(g_users[i].name, name) == 0) return i;
    return -1;
}

/**
 * Přidá nového uživatele do interní tabulky.
 *
 * @param name Uživatelské jméno (const char *)
 * @param pass Heslo, může být NULL nebo prázdné (const char *)
 * @return     0 při úspěchu, -1 při chybě (int)
 */
int auth_add_user(const char *name, const char *pass) {
    usize nlen, plen;
    if (!name || name[0] == '\0' || g_user_count >= AUTH_MAX_USERS) return -1;
    if (auth_find_user(name) >= 0) return -1;

    nlen = aster_strlen(name);
    plen = pass ? aster_strlen(pass) : 0;
    if (nlen >= AUTH_NAME_LEN || plen >= AUTH_PASS_LEN) return -1;

    aster_memset(g_users[g_user_count].name, 0, AUTH_NAME_LEN);
    aster_memset(g_users[g_user_count].pass, 0, AUTH_PASS_LEN);
    aster_memcpy(g_users[g_user_count].name, name, nlen);
    if (pass) aster_memcpy(g_users[g_user_count].pass, pass, plen);
    ++g_user_count;
    return 0;
}

/**
 * Uloží uživatele z interní tabulky do souboru "/.users" na disku.
 *
 * @return 0 při úspěchu, -1 při chybě (int)
 */
int auth_save_users(void) {
    char buf[4096];
    usize pos = 0;
    int i;
    aster_memset(buf, 0, sizeof(buf));

    for (i = 0; i < g_user_count; ++i) {
        usize nlen = aster_strlen(g_users[i].name);
        usize plen = aster_strlen(g_users[i].pass);
        if (pos + nlen + 1 + plen + 1 >= sizeof(buf)) return -1;
        aster_memcpy(buf + pos, g_users[i].name, nlen);
        pos += nlen; buf[pos++] = ':';
        aster_memcpy(buf + pos, g_users[i].pass, plen);
        pos += plen; buf[pos++] = '\n';
    }
    return asterfs_write_file(AUTH_USERS_FILE, (const u8 *)buf, (u16)pos) >= 0 ? 0 : -1;
}

/**
 * Načte uživatele ze souboru "/.users" do interní tabulky.
 *
 * @return 0 při úspěchu, -1 při chybě (int)
 */
int auth_load_users(void) {
    char buf[4097];
    int n;
    usize i = 0;
    auth_clear_users();

    n = asterfs_read_file(AUTH_USERS_FILE, (u8 *)buf, 4096);
    if (n < 0) return -1;
    buf[n] = '\0';

    while (i < (usize)n) {
        char name[AUTH_NAME_LEN], pass[AUTH_PASS_LEN];
        usize ni = 0, pi = 0;
        while (i < (usize)n && (buf[i] == '\n' || buf[i] == '\r')) ++i;
        if (i >= (usize)n) break;
        while (i < (usize)n && buf[i] != ':' && buf[i] != '\n' && ni < AUTH_NAME_LEN - 1)
            name[ni++] = buf[i++];
        name[ni] = '\0';
        if (i < (usize)n && buf[i] == ':') ++i;
        while (i < (usize)n && buf[i] != '\n' && pi < AUTH_PASS_LEN - 1)
            pass[pi++] = buf[i++];
        pass[pi] = '\0';
        while (i < (usize)n && buf[i] != '\n') ++i;
        if (i < (usize)n && buf[i] == '\n') ++i;
        if (name[0] && auth_add_user(name, pass) != 0) return -1;
    }
    return 0;
}

/**
 * Nastaví nové heslo existujícímu uživateli.
 *
 * @param user Uživatelské jméno (const char *)
 * @param pass Nové heslo (const char *)
 * @return     0 při úspěchu, -1 při chybě (int)
 */
int auth_set_pass(const char *user, const char *pass) {
    int idx = auth_find_user(user);
    usize plen;
    if (idx < 0) return -1;
    plen = pass ? aster_strlen(pass) : 0;
    if (plen >= AUTH_PASS_LEN) return -1;
    aster_memset(g_users[idx].pass, 0, AUTH_PASS_LEN);
    if (pass) aster_memcpy(g_users[idx].pass, pass, plen);
    return 0;
}

/**
 * Najde uživatele s prázdným heslem (auto-login).
 *
 * @return Index uživatele, nebo -1 pokud žádný není (int)
 */
int auth_find_autologin_user(void) {
    int i;
    for (i = 0; i < g_user_count; ++i)
        if (g_users[i].pass[0] == '\0') return i;
    return -1;
}

/**
 * Zobrazí přihlašovací obrazovku.
 * Pokud systém není nainstalován, přihlásí se jako "guest".
 * Pokud existuje uživatel s prázdným heslem, provede auto-login.
 * Jinak cyklicky žádá o jméno a heslo, dokud není úspěšné přihlášení.
 */
void auth_login_screen(void) {
    char name[AUTH_NAME_LEN], pass[AUTH_PASS_LEN];
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

        int idx = auth_find_user(name);
        if (idx >= 0 && aster_strcmp(g_users[idx].pass, pass) == 0) {
            aster_memset(g_current_user, 0, sizeof(g_current_user));
            aster_memcpy(g_current_user, g_users[idx].name, aster_strlen(g_users[idx].name));
            return;
        }
        print_error("Spatny login nebo heslo, zkus to znovu...");
        timer_sleep_ms(4000);
    }
}
