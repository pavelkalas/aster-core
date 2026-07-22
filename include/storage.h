/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header definuje veřejné rozhraní jednoduchého AsterFS.
 * Popisuje formát uzlu souboru nebo adresáře a deklaruje operace
 * pro inicializaci, vytváření, čtení, zápis a výpis obsahu.
 */

#ifndef ASTER_STORAGE_H
#define ASTER_STORAGE_H

#include "types.h"

/** Maximální počet souborů v systému. */
#define ASTERFS_MAX_FILES 64
/** Maximální délka názvu souboru (včetně cesty). */
#define ASTERFS_NAME_LEN  64
/** Maximální délka dat v souboru. */
#define ASTERFS_DATA_LEN  512

/** Struktura uzlu souboru/adresáře v paměti. */
typedef struct {
    char name[ASTERFS_NAME_LEN];   /**< Plná cesta (např. "/soubor.txt"). */
    u8 is_dir;                     /**< 1 pro adresář, 0 pro soubor. */
    u16 size;                      /**< Velikost dat v bajtech. */
    u8 data[ASTERFS_DATA_LEN];     /**< Data souboru. */
} asterfs_node_t;

/** Inicializuje AsterFS – detekuje disk a načte data. */
void asterfs_init(void);

/** Vytvoří soubor. */
int asterfs_create_file(const char *name);
/** Vytvoří adresář. */
int asterfs_create_dir(const char *name);
/** Smaže soubor. */
int asterfs_remove_file(const char *name);
/** Smaže adresář. */
int asterfs_remove_dir(const char *name);
/** Zapíše data do souboru. */
int asterfs_write_file(const char *name, const u8 *data, u16 len);
/** Přečte data ze souboru. */
int asterfs_read_file(const char *name, u8 *out, u16 max_len);
/** Získá typ uzlu (0=soubor, 1=adresář, -1=neexistuje). */
int asterfs_get_type(const char *name);
/** Synchronizuje FS na disk. */
int asterfs_sync(void);
/** Projde všechny uzly FS a zavolá callback pro každý. */
void asterfs_list(void (*cb)(const char *name, u8 is_dir, u16 size));
/** Vypíše obsah adresáře (callback pro každého potomka). */
void asterfs_list_dir(const char *path, void (*cb)(const char *name, u8 is_dir, u16 size));

#endif
