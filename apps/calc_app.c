/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Ukazkova C aplikace nad Aster API. Aplikace predvadi jednoduchou
 * kalkulacku a muze slouzit jako sablona pro dalsi user-space programy.
 */

#include "aster_api.h"

static void append_number(char *out, int value) {
    char tmp[16];
    int i = 0;
    int j;
    int neg = 0;

    if (value == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    if (value < 0) {
        neg = 1;
        value = -value;
    }

    while (value > 0 && i < 15) {
        tmp[i++] = (char)('0' + (value % 10));
        value /= 10;
    }

    j = 0;
    if (neg) {
        out[j++] = '-';
    }

    while (i > 0) {
        out[j++] = tmp[--i];
    }

    out[j] = '\0';
}

void app_calc_main(void) {
    char n1[16];
    char n2[16];
    char nr[16];
    int a = 24;
    int b = 18;
    int r = a + b;

    append_number(n1, a);
    append_number(n2, b);
    append_number(nr, r);

    aster_api_print("[calc_app] start\n");
    aster_api_print("[calc_app] ");
    aster_api_print(n1);
    aster_api_print(" + ");
    aster_api_print(n2);
    aster_api_print(" = ");
    aster_api_print(nr);
    aster_api_print("\n");
}
