/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 *
 * Jednoduché pomocné funkce pro parsování čísel z řetězců.
 */

#include "int.h"

/**
 * Převede řetězec na unsigned long (dekadicky).
 *
 * @param s  Řetězec s číslem (const char *)
 * @param ok Výstup – 1 při úspěšném parsování, 0 při chybě (int *)
 * @return   Přečtená hodnota (unsigned long)
 */
unsigned long parse_u32(const char *s, int *ok) {
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

/**
 * Převede řetězec na signed long (dekadicky, podporuje znaménko '-').
 *
 * @param s  Řetězec s číslem (const char *)
 * @param ok Výstup – 1 při úspěšném parsování, 0 při chybě (int *)
 * @return   Přečtená hodnota (long)
 */
long parse_i32(const char *s, int *ok) {
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