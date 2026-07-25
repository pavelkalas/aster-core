/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: alloc – Alokuje paměť na haldě.
 * Použití: alloc <bajty>
 */

#include "aster_api.h"

void init(void) {
    int argc = aster_api_get_argc();
    const char *s;
    unsigned long n;
    void *p;

    if (argc < 2) {
        aster_api_print_line("Pouziti: alloc <bajty>");
        return;
    }

    s = aster_api_get_argv(1);
    if (!s) return;

    n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (unsigned long)(*s - '0');
        ++s;
    }

    p = aster_api_alloc((usize)n);
    {
        char buf[32];
        const char *pre = "alloc -> 0x";
        int pos = 0;
        while (*pre) buf[pos++] = *pre++;
        {
            /* hex print */
            unsigned long long v = (unsigned long long)(usize)p;
            char tmp[24]; int tp = 0;
            if (v == 0) tmp[tp++] = '0';
            else {
                while (v > 0) {
                    unsigned int d = (unsigned int)(v % 16);
                    tmp[tp++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
                    v /= 16;
                }
            }
            while (tp > 0) buf[pos++] = tmp[--tp];
        }
        buf[pos++] = '\n';
        buf[pos] = '\0';
        aster_api_print(buf);
    }
}
