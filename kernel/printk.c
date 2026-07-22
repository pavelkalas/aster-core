/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento modul implementuje tiskovou vrstvu kernelu nad VGA výstupem.
 * Obsahuje podporu základních formátovacích specifikátorů pro ladění
 * a jednotné API, které používají ostatní části jádra při logování.
 */

#include <stdarg.h>

#include "display.h"
#include "printk.h"
#include "serial.h"

/**
 * Vypíše jeden znak na výstup.
 * Znak je odeslán jak na VGA display, tak na sériovou linku.
 *
 * @param c Znak k vypsání (char)
 */
static void printk_putc(char c) {
    display_putc(c);
    serial_write_char(c);
}

/**
 * Vypíše řetězec znaků na výstup.
 * Pokud je předaný ukazatel NULL, vypíše se místo něj "(null)".
 *
 * @param s Ukazatel na null-terminovaný řetězec (const char *)
 */
static void printk_puts(const char *s) {
    if (!s) {
        s = "(null)";
    }

    while (*s) {
        printk_putc(*s++);
    }
}

/**
 * Vypíše celé nezáporné číslo v požadované číselné soustavě.
 * Používá se jako pomocná funkce pro formátování čísel.
 * Podporuje soustavy od 2 do 16 (číslice 0-9, a-f).
 *
 * @param value  Hodnota k vypsání (unsigned long long)
 * @param base   Číselná soustava (unsigned int), např. 10 pro desítkovou, 16 pro hex
 */
static void print_u64(unsigned long long value, unsigned int base) {
    char buf[32];
    const char *digits = "0123456789abcdef";
    int i = 0;

    if (value == 0) {
        printk_putc('0');
        return;
    }

    while (value > 0 && i < (int)sizeof(buf)) {
        buf[i++] = digits[value % base];
        value /= base;
    }

    while (i > 0) {
        printk_putc(buf[--i]);
    }
}

/**
 * Vypíše text na výstup (jednoduchá obálka pro tisk řetězce).
 *
 * @param text Ukazatel na null-terminovaný řetězec (const char *)
 */
void aster_print(const char *text) {
    printk_puts(text);
}

/**
 * Kernelová obdoba printf – formátovaný výstup na obrazovku a sériovou linku.
 * Podporuje základní formátovací specifikátory:
 *   %%  – vypsání znaku '%'
 *   %c  – jeden znak (int se převede na char)
 *   %s  – řetězec (const char *)
 *   %d  – celé číslo se znaménkem (int)
 *   %u  – bezneznaménkové celé číslo (unsigned int)
 *   %x  – hexadecimální číslo (unsigned int)
 *   %p  – adresa/ukazatel (void *), vždy s prefixem "0x"
 *
 * @param fmt  Formátovací řetězec (const char *)
 * @param ...  Variabilní argumenty odpovídající formátovacím specifikátorům
 */
void printk(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            printk_putc(*fmt++);
            continue;
        }

        ++fmt;
        switch (*fmt) {
            case '%':
                printk_putc('%');
                break;
            case 'c': {
                int c = va_arg(args, int);
                printk_putc((char)c);
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                printk_puts(s);
                break;
            }
            case 'd': {
                long v = va_arg(args, int);
                if (v < 0) {
                    printk_putc('-');
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
                printk_puts("0x");
                print_u64(v, 16);
                break;
            }
            default:
                printk_putc('?');
                break;
        }
        ++fmt;
    }

    va_end(args);
}
