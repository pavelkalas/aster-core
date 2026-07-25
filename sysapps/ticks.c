/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: ticks – Vypíše počet tiknutí časovače.
 * Použití: ticks
 */

#include "aster_api.h"

void init(void) {
    u64 ticks;
    aster_api_ticks(&ticks);
    {
        char buf[32];
        int pos = 0;
        const char *pre = "ticks=";
        while (*pre) buf[pos++] = *pre++;
        {
            char tmp[24]; int tp = 0; u64 n = ticks;
            if (n == 0) tmp[tp++] = '0';
            else { while (n > 0) { tmp[tp++] = (char)('0' + (unsigned int)(n % 10)); n /= 10; } }
            while (tp > 0) buf[pos++] = tmp[--tp];
        }
        buf[pos++] = '\n';
        buf[pos] = '\0';
        aster_api_print(buf);
    }
}
