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
