/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavickovy soubor autentizacniho modulu.
 * Definuje strukturu uzivatele, max. pocty, a vsechny funkce
 * pro praci s uzivateli — vytvareni, mazani, ukladani, login.
 */

#ifndef ASTER_AUTH_H
#define ASTER_AUTH_H

#include "types.h"

#define AUTH_MAX_USERS 16
#define AUTH_NAME_LEN 32
#define AUTH_PASS_LEN 32

typedef struct {
    char name[AUTH_NAME_LEN];
    char pass[AUTH_PASS_LEN];
} auth_user_t;

extern char g_current_user[AUTH_NAME_LEN];
extern usize g_current_user_size;

void auth_readline_plain(char *out, int max_len);
void auth_readline_secret(char *out, int max_len);
void auth_clear_users(void);
int  auth_find_user(const char *name);
int  auth_add_user(const char *name, const char *pass);
int  auth_save_users(void);
int  auth_load_users(void);
int  auth_set_pass(const char *user, const char *pass);
int  auth_find_autologin_user(void);
void auth_login_screen(void);

#endif
