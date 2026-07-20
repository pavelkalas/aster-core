/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavickovy soubor vestaveneho textoveho editoru.
 * Editor je ovladan klavesnici, podporuje Ctrl+S (ulozeni)
 * a ESC (konec bez ulozeni).
 */

#ifndef ASTER_EDITOR_H
#define ASTER_EDITOR_H

void shell_edit_file(const char *path);

#endif
