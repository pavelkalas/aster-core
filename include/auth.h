/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavičkový soubor autentizačního modulu.
 * Definuje strukturu uživatele, max. počty a všechny funkce
 * pro práci s uživateli – vytváření, mazání, ukládání, login.
 */

#ifndef ASTER_AUTH_H
#define ASTER_AUTH_H

#include "types.h"

/** Maximální počet uživatelů v tabulce. */
#define AUTH_MAX_USERS 16
/** Maximální délka uživatelského jména. */
#define AUTH_NAME_LEN 32
/** Maximální délka hesla. */
#define AUTH_PASS_LEN 32

/** Struktura uživatele – jméno a heslo. */
typedef struct {
    char name[AUTH_NAME_LEN];  /**< Uživatelské jméno. */
    char pass[AUTH_PASS_LEN];  /**< Heslo. */
} auth_user_t;

/** Jméno aktuálně přihlášeného uživatele. */
extern char g_current_user[AUTH_NAME_LEN];
extern usize g_current_user_size;

/** Přečte řádek textu z klávesnice (viditelný). */
void auth_readline_plain(char *out, int max_len);
/** Přečte řádek textu z klávesnice (neviditelný, pro hesla). */
void auth_readline_secret(char *out, int max_len);
/** Smaže všechny uživatele z tabulky. */
void auth_clear_users(void);
/** Najde uživatele podle jména. */
int  auth_find_user(const char *name);
/** Přidá nového uživatele. */
int  auth_add_user(const char *name, const char *pass);
/** Uloží uživatele na disk. */
int  auth_save_users(void);
/** Načte uživatele z disku. */
int  auth_load_users(void);
/** Nastaví nové heslo. */
int  auth_set_pass(const char *user, const char *pass);
/** Najde uživatele s prázdným heslem (auto-login). */
int  auth_find_autologin_user(void);
/** Zobrazí přihlašovací obrazovku. */
void auth_login_screen(void);

#endif
