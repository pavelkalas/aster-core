/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: memory – Vypíše stav paměti.
 * Použití: memory
 */

#include "aster_api.h"

void init(void) {
    aster_api_memory_info_t info;
    if (aster_api_memory_info(&info) == 0) {
        char buf[64];
        const char *pre = "total pages=";
        int pos = 0;
        while (*pre) buf[pos++] = *pre++;

        /* append total_pages */
        {
            char tmp[16]; int tp = 0; usize n = info.total_pages;
            if (n == 0) tmp[tp++] = '0';
            else { while (n > 0) { tmp[tp++] = (char)('0' + (unsigned int)(n % 10)); n /= 10; } }
            while (tp > 0) buf[pos++] = tmp[--tp];
        }

        {
            const char *mid = " free pages=";
            while (*mid) buf[pos++] = *mid++;
        }

        /* append free_pages */
        {
            char tmp[16]; int tp = 0; usize n = info.free_pages;
            if (n == 0) tmp[tp++] = '0';
            else { while (n > 0) { tmp[tp++] = (char)('0' + (unsigned int)(n % 10)); n /= 10; } }
            while (tp > 0) buf[pos++] = tmp[--tp];
        }

        buf[pos++] = '\n';
        buf[pos] = '\0';
        aster_api_print(buf);
    }
}
