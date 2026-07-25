/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: process – Vypíše seznam všech procesů.
 * Použití: process
 */

#include "aster_api.h"

void init(void) {
    usize count;
    aster_api_process_count(&count);
    if (count == 0) {
        aster_api_print_line("Zadne procesy");
        return;
    }
    {
        usize i;
        for (i = 0; i < count; ++i) {
            aster_api_process_info_t p;
            if (aster_api_process_get(i, &p) == 0) {
                const char *state = "UNK";
                char buf[80];
                int pos = 0;
                const char *pre = "pid=";
                while (*pre) buf[pos++] = *pre++;
                /* pid */
                {
                    char tmp[12]; int tp = 0; u32 n = p.pid;
                    if (n == 0) tmp[tp++] = '0';
                    else { while (n > 0) { tmp[tp++] = (char)('0' + (n % 10)); n /= 10; } }
                    while (tp > 0) buf[pos++] = tmp[--tp];
                }
                {
                    const char *s2 = " state=";
                    while (*s2) buf[pos++] = *s2++;
                }
                if (p.state == 1) state = "READY";
                else if (p.state == 2) state = "RUN";
                else if (p.state == 3) state = "BLOCK";
                else if (p.state == 4) state = "EXIT";
                { const char *s3 = state; while (*s3) buf[pos++] = *s3++; }
                {
                    const char *s4 = " prio=";
                    while (*s4) buf[pos++] = *s4++;
                }
                {
                    char tmp[8]; int tp = 0; u8 n = p.priority;
                    if (n == 0) tmp[tp++] = '0';
                    else { while (n > 0) { tmp[tp++] = (char)('0' + (n % 10)); n /= 10; } }
                    while (tp > 0) buf[pos++] = tmp[--tp];
                }
                {
                    const char *s5 = " name=";
                    while (*s5) buf[pos++] = *s5++;
                }
                { const char *s6 = p.name ? p.name : "-"; while (*s6) buf[pos++] = *s6++; }
                buf[pos++] = '\n';
                buf[pos] = '\0';
                aster_api_print(buf);
            }
        }
    }
}
