/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor obsahuje vlastní implementace základních utility funkcí.
 * Kernel je používá místo standardní knihovny pro strlen, strcmp,
 * memcpy a memset, aby zůstal freestanding a plně pod kontrolou jádra.
 */

#include "string.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

/**
 * Zjistí, zda je znak whitespace (mezera, tab, nový řádek, atd.).
 *
 * @param c Znak (char)
 * @return  1 pokud je whitespace, jinak 0 (int)
 */
static inline int aster_isspace(char c) {
    return (c == ' ' || c == '\t' || c == '\n' ||
            c == '\r' || c == '\v' || c == '\f');
}

/**
 * Vrátí délku řetězce (počet znaků před null terminátorem).
 *
 * @param s Řetězec (const char *)
 * @return  Počet znaků (usize)
 */
usize aster_strlen(const char *s) {
    usize n = 0;
    while (s && s[n]) {
        ++n;
    }
    return n;
}

/**
 * Porovná dva řetězce lexikograficky.
 *
 * @param a První řetězec (const char *)
 * @param b Druhý řetězec (const char *)
 * @return  0 pokud jsou stejné, záporné číslo pokud a < b, kladné pokud a > b (int)
 */
int aster_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        ++a;
        ++b;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/**
 * Porovná první n znaků dvou řetězců.
 *
 * @param a První řetězec (const char *)
 * @param b Druhý řetězec (const char *)
 * @param n Maximální počet znaků k porovnání (usize)
 * @return  0 pokud jsou stejné, jinak rozdíl prvních odlišných znaků (int)
 */
int aster_strncmp(const char *a, const char *b, usize n) {
    usize i;

    for (i = 0; i < n; ++i) {
        if (a[i] != b[i] || a[i] == '\0' || b[i] == '\0') {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }
    }

    return 0;
}

/**
 * Zkopíruje n bajtů z paměti src do dst.
 *
 * @param dst Cílový ukazatel (void *)
 * @param src Zdrojový ukazatel (const void *)
 * @param n   Počet bajtů (usize)
 * @return    Ukazatel na dst (void *)
 */
void *aster_memcpy(void *dst, const void *src, usize n) {
    usize i;
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;

    for (i = 0; i < n; ++i) {
        d[i] = s[i];
    }

    return dst;
}

/**
 * Nastaví n bajtů paměti na hodnotu v.
 *
 * @param dst Ukazatel na paměť (void *)
 * @param v   Hodnota (int, interně konvertovaná na u8)
 * @param n   Počet bajtů (usize)
 * @return    Ukazatel na dst (void *)
 */
void *aster_memset(void *dst, int v, usize n) {
    usize i;
    u8 *d = (u8 *)dst;

    for (i = 0; i < n; ++i) {
        d[i] = (u8)v;
    }

    return dst;
}

/**
 * Zkopíruje řetězec src do dst (včetně null terminátoru).
 *
 * @param dst Cílový buffer (char *)
 * @param src Zdrojový řetězec (const char *)
 * @return    Ukazatel na dst (char *)
 */
char *aster_strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++) != '\0')
        ;
    return dst;
}

/**
 * Najde první výskyt podřetězce needle v řetězci haystack.
 *
 * @param haystack Hledaný řetězec (const char *)
 * @param needle   Hledaný podřetězec (const char *)
 * @return         Ukazatel na první výskyt, nebo NULL (char *)
 */
char *aster_strstr(const char *haystack, const char *needle) {
    if (!*needle)
        return (char *)haystack;

    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && (*h == *n)) {
            ++h;
            ++n;
        }
        if (!*n)
            return (char *)haystack;
        ++haystack;
    }
    return NULL;
}

/**
 * Odstraní bílé znaky z obou konců řetězce a vrátí ukazatel na začátek.
 *
 * @param str Řetězec k oříznutí (char *)
 * @return    Ukazatel na začátek oříznutého řetězce (char *)
 */
char *aster_trim(char *str) {
    if (!str)
        return NULL;

    while (*str && aster_isspace(*str))
        ++str;

    char *end = str;
    while (*end)
        ++end;
    while (end > str && aster_isspace(*(end - 1)))
        --end;
    *end = '\0';

    return str;
}

/**
 * Extrahuje podřetězec z řetězce od indexu start o délce length.
 *
 * @param dst     Cílový buffer (char *)
 * @param dstsize Velikost cílového bufferu (usize)
 * @param src     Zdrojový řetězec (const char *)
 * @param start   Počáteční index (usize)
 * @param length  Maximální délka podřetězce (usize)
 * @return        Počet zkopírovaných znaků, nebo -1 při chybě (int)
 */
int aster_substring(char *dst, usize dstsize, const char *src,
                    usize start, usize length) {
    if (!dst || dstsize == 0 || !src)
        return -1;

    usize srclen = aster_strlen(src);
    if (start >= srclen) {
        dst[0] = '\0';
        return 0;
    }

    usize max_copy = srclen - start;
    if (length > max_copy)
        length = max_copy;
    if (length >= dstsize)
        length = dstsize - 1;

    for (usize i = 0; i < length; ++i)
        dst[i] = src[start + i];
    dst[length] = '\0';
    return (int)length;
}

/**
 * Odstraní z řetězce všechny výskyty podřetězce substr.
 *
 * @param str    Řetězec k úpravě (char *)
 * @param substr Podřetězec k odstranění (const char *)
 * @return       Ukazatel na upravený řetězec (char *)
 */
char *aster_remove(char *str, const char *substr) {
    if (!str || !substr || !*substr)
        return str;

    usize sublen = aster_strlen(substr);
    char *src = str;
    char *dst = str;

    while (*src) {
        const char *s = src;
        const char *sub = substr;
        while (*s && *sub && (*s == *sub)) {
            ++s;
            ++sub;
        }
        if (!*sub) {
            src += sublen;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return str;
}

/**
 * Rozdělí řetězec na tokeny podle oddělovače.
 * Pokud je tokens NULL, pouze spočítá počet tokenů.
 *
 * @param str       Řetězec k rozdělení (bude modifikován) (char *)
 * @param delim     Oddělovač (const char *)
 * @param tokens    Pole pro ukazatele na tokeny, nebo NULL (char **)
 * @param max_tokens Maximální počet tokenů (usize)
 * @return          Počet tokenů, nebo -1 při chybě (int)
 */
int aster_split(char *str, const char *delim, char **tokens, usize max_tokens) {
    if (!str || !delim || !*delim)
        return -1;

    usize delim_len = aster_strlen(delim);
    int count = 0;
    char *scan = str;

    if (!tokens) {
        if (*scan == '\0')
            return 0;
        while (*scan) {
            if (count == 0) {
                ++count;
            }
            char *found = aster_strstr(scan, delim);
            if (found) {
                ++count;
                scan = found + delim_len;
            } else {
                break;
            }
        }
        return count;
    }

    while (*scan && (usize)count < max_tokens) {
        tokens[count++] = scan;
        char *found = aster_strstr(scan, delim);
        if (found) {
            *found = '\0';
            scan = found + delim_len;
        } else {
            break;
        }
    }

    if (*scan && (usize)count < max_tokens) {
        tokens[count++] = scan;
    }

    return count;
}

/**
 * Zjistí, zda řetězec obsahuje podřetězec.
 *
 * @param str    Hledaný řetězec (const char *)
 * @param substr Hledaný podřetězec (const char *)
 * @return       1 pokud obsahuje, jinak 0 (int)
 */
int aster_contains(const char *str, const char *substr) {
    return aster_strstr(str, substr) != NULL;
}

/**
 * Zjistí, zda řetězec začíná daným prefixem.
 *
 * @param str    Řetězec (const char *)
 * @param prefix Hledaný prefix (const char *)
 * @return       1 pokud ano, jinak 0 (int)
 */
int aster_starts_with(const char *str, const char *prefix) {
    if (!str || !prefix)
        return 0;
    while (*prefix) {
        if (*str != *prefix)
            return 0;
        ++str;
        ++prefix;
    }
    return 1;
}

/**
 * Zjistí, zda řetězec končí daným suffixem.
 *
 * @param str    Řetězec (const char *)
 * @param suffix Hledaný suffix (const char *)
 * @return       1 pokud ano, jinak 0 (int)
 */
int aster_ends_with(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    usize str_len = aster_strlen(str);
    usize suffix_len = aster_strlen(suffix);
    if (suffix_len > str_len)
        return 0;
    return aster_strcmp(str + str_len - suffix_len, suffix) == 0;
}
