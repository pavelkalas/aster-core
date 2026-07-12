/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento modul implementuje tiskovou vrstvu kernelu nad VGA vystupem.
 * Obsahuje podporu zakladnich formatovacich specifikatoru pro ladeni
 * a jednotne API, ktere pouzivaji ostatni casti jadra pri logovani.
 */

#include <stdarg.h>

#include "display.h"
#include "printk.h"

static void print_u64(unsigned long long value, unsigned int base) {
    char buf[32];
    const char *digits = "0123456789abcdef";
    int i = 0;

    if (value == 0) {
        display_putc('0');
        return;
    }

    while (value > 0 && i < (int)sizeof(buf)) {
        buf[i++] = digits[value % base];
        value /= base;
    }

    while (i > 0) {
        display_putc(buf[--i]);
    }
}

void aster_print(const char *text) {
    display_write(text);
}

void printk(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            display_putc(*fmt++);
            continue;
        }

        ++fmt;
        switch (*fmt) {
            case '%':
                display_putc('%');
                break;
            case 'c': {
                int c = va_arg(args, int);
                display_putc((char)c);
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) {
                    s = "(null)";
                }
                display_write(s);
                break;
            }
            case 'd': {
                long v = va_arg(args, int);
                if (v < 0) {
                    display_putc('-');
                    print_u64((unsigned long long)(-v), 10);
                } else {
                    print_u64((unsigned long long)v, 10);
                }
                break;
            }
            case 'u': {
                unsigned int v = va_arg(args, unsigned int);
                print_u64(v, 10);
                break;
            }
            case 'x': {
                unsigned int v = va_arg(args, unsigned int);
                print_u64(v, 16);
                break;
            }
            case 'p': {
                unsigned long long v = (unsigned long long)va_arg(args, void *);
                display_write("0x");
                print_u64(v, 16);
                break;
            }
            default:
                display_putc('?');
                break;
        }
        ++fmt;
    }

    va_end(args);
}
