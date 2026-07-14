/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor obsahuje vlastni implementace zakladnich utility funkci.
 * Kernel je pouziva misto standardni knihovny pro strlen, strcmp,
 * memcpy a memset, aby zustal freestanding a plne pod kontrolou jadra.
 */

#include "string.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

static inline int aster_isspace(char c) {
    return (c == ' ' || c == '\t' || c == '\n' ||
            c == '\r' || c == '\v' || c == '\f');
}

usize aster_strlen(const char *s) {
    usize n = 0;
    while (s && s[n]) {
        ++n;
    }
    return n;
}

int aster_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        ++a;
        ++b;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int aster_strncmp(const char *a, const char *b, usize n) {
    usize i;

    for (i = 0; i < n; ++i) {
        if (a[i] != b[i] || a[i] == '\0' || b[i] == '\0') {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }
    }

    return 0;
}

void *aster_memcpy(void *dst, const void *src, usize n) {
    usize i;
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;

    for (i = 0; i < n; ++i) {
        d[i] = s[i];
    }

    return dst;
}

void *aster_memset(void *dst, int v, usize n) {
    usize i;
    u8 *d = (u8 *)dst;

    for (i = 0; i < n; ++i) {
        d[i] = (u8)v;
    }

    return dst;
}

char *aster_strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++) != '\0')
        ;
    return dst;
}

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

int aster_contains(const char *str, const char *substr) {
    return aster_strstr(str, substr) != NULL;
}

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

int aster_ends_with(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    usize str_len = aster_strlen(str);
    usize suffix_len = aster_strlen(suffix);
    if (suffix_len > str_len)
        return 0;
    return aster_strcmp(str + str_len - suffix_len, suffix) == 0;
}
