/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header definuje verejne rozhrani jednoducheho AsterFS.
 * Popisuje format uzlu souboru nebo adresare a deklaruje operace
 * pro inicializaci, vytvareni, cteni, zapis a vypis obsahu.
 */

#ifndef ASTER_STORAGE_H
#define ASTER_STORAGE_H

#include "types.h"

#define ASTERFS_MAX_FILES 64
#define ASTERFS_NAME_LEN  64
#define ASTERFS_DATA_LEN  512

typedef struct {
    char name[ASTERFS_NAME_LEN];
    u8 is_dir;
    u16 size;
    u8 data[ASTERFS_DATA_LEN];
} asterfs_node_t;

void asterfs_init(void);
int asterfs_create_file(const char *name);
int asterfs_create_dir(const char *name);
int asterfs_remove_file(const char *name);
int asterfs_remove_dir(const char *name);
int asterfs_write_file(const char *name, const u8 *data, u16 len);
int asterfs_read_file(const char *name, u8 *out, u16 max_len);
int asterfs_get_type(const char *name);
void asterfs_list(void (*cb)(const char *name, u8 is_dir, u16 size));
void asterfs_list_dir(const char *path, void (*cb)(const char *name, u8 is_dir, u16 size));

#endif
