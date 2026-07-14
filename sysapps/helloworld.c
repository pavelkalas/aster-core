#include "aster_api.h"
#include "display.h"
#include "printk.h"

void init(void) {
    for (int col = 0x01; col <= 0x0F; col++) {
        display_set_color(col, 0x00);
        printk("Hello, world! (0x0%d)\n", col);
    }

    aster_api_print("\n");
    display_set_color(0x0F, 0x00);
}
