/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Sysapp: fm – Spustí interaktivní file manager.
 * Použití: fm
 */

#include "aster_api.h"

void init(void) {
    aster_api_run_file_manager();
    aster_api_render_statusbar();
}
