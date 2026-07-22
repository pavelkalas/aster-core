/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor deklaruje minimalistické utility bez závislosti na libc.
 * Obsahuje základní operace nad řetězci a pamětí, které kernel potřebuje
 * již v raných fázích bootu, kdy standardní knihovny nejsou dostupné.
 */

#ifndef ASTER_STRING_H
#define ASTER_STRING_H

#include "types.h"

/** Vrátí délku řetězce. */
usize aster_strlen(const char *s);

/** Porovná dva řetězce lexikograficky. */
int aster_strcmp(const char *a, const char *b);

/** Porovná prvních n znaků dvou řetězců. */
int aster_strncmp(const char *a, const char *b, usize n);

/** Zkopíruje n bajtů z paměti src do dst. */
void *aster_memcpy(void *dst, const void *src, usize n);

/** Nastaví n bajtů paměti na hodnotu v. */
void *aster_memset(void *dst, int v, usize n);

/** Najde první výskyt podřetězce needle v haystack. */
char *aster_strstr(const char *haystack, const char *needle);

/** Odstraní bílé znaky z obou konců řetězce. */
char *aster_trim(char *str);

/** Extrahuje podřetězec od start o délce length. */
int aster_substring(char *dst, usize dstsize, const char *src, usize start, usize length);

/** Odstraní z řetězce všechny výskyty substr. */
char *aster_remove(char *str, const char *substr);

/** Rozdělí řetězec na tokeny podle oddělovače. */
int aster_split(char *str, const char *delim, char **tokens, usize max_tokens);

/** Zjistí, zda řetězec obsahuje podřetězec. */
int aster_contains(const char *str, const char *substr);

/** Zjistí, zda řetězec začíná daným prefixem. */
int aster_starts_with(const char *str, const char *prefix);

/** Zjistí, zda řetězec končí daným suffixem. */
int aster_ends_with(const char *str, const char *suffix);

/** Zkopíruje řetězec src do dst. */
char *aster_strcpy(char *dst, const char *src);

#endif
