/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavičkový soubor vestavěného textového editoru.
 * Editor je ovládán klávesnicí, podporuje Ctrl+S (uložení)
 * a ESC (konec bez uložení).
 */

#ifndef ASTER_EDITOR_H
#define ASTER_EDITOR_H

/** Spustí textový editor pro soubor na zadané cestě. */
void shell_edit_file(const char *path);

#endif
