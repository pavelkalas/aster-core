/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavičkový soubor FS utility.
 * Definuje dočasný buffer pro hromadné FS operace (FS_TMP_MAX)
 * a deklaruje funkce pro práci s cestami, kopírování, mazání
 * adresářových stromů a zajištění existence adresářů/souborů.
 */

#ifndef ASTER_FS_UTILS_H
#define ASTER_FS_UTILS_H

#include "types.h"

/** Maximální počet položek v dočasném bufferu. */
#define FS_TMP_MAX 64

/** Dočasné pole cest. */
extern char g_fs_tmp_paths[FS_TMP_MAX][64];
/** Dočasné pole typů (soubor/adresář). */
extern u8   g_fs_tmp_types[FS_TMP_MAX];
/** Počet položek v dočasném bufferu. */
extern int  g_fs_tmp_count;

/** Zajistí existenci adresáře. */
int  fs_ensure_dir(const char *path);
/** Zajistí existenci textového souboru. */
int  fs_ensure_file_text(const char *path, const char *text);
/** Zjistí, zda je systém nainstalován. */
int  system_is_installed(void);

/** Přeloží relativní cestu na absolutní (vůči g_cwd). */
void resolve_path(const char *name, char *out, usize out_size);
/** Přeloží relativní cestu na absolutní (vůči zadanému base). */
void resolve_path_from(const char *base, const char *name, char *out, usize out_size);
/** Vrátí název souboru z cesty (za posledním '/'). */
const char *path_basename(const char *path);

/** Zjistí, zda je path potomkem root (nebo stejný). */
int  path_is_self_or_child(const char *root, const char *path);
/** Namapuje cestu z jednoho kořene na jiný. */
int  map_path_prefix(const char *src_root, const char *dst_root, const char *path, char *out, usize out_size);

/** Zpětné volání pro sběr položek FS. */
void fs_collect_cb(const char *name, u8 is_dir, u16 size);
/** Sběr všech položek FS do dočasných polí. */
void fs_collect_all(void);
/** Zjistí, zda má adresář potomky. */
int  fs_dir_has_children(const char *path);

/** Zkopíruje soubor (absolutní cesty). */
int  fs_copy_file_abs(const char *src, const char *dst);
/** Zkopíruje adresář (absolutní cesty, volitelně rekurzivně). */
int  fs_copy_dir_abs(const char *src, const char *dst, int recursive);
/** Rekurzivně smaže adresářový strom. */
int  fs_remove_tree_abs(const char *root);

#endif
