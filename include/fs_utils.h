/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Hlavickovy soubor FS utility.
 * Definuje docasny buffer pro hromadne FS operace (FS_TMP_MAX)
 * a deklaruje funkce pro praci s cestami, kopirovani, mazani
 * adresarovych stromu a zajisteni existence adresaru/souboru.
 */

#ifndef ASTER_FS_UTILS_H
#define ASTER_FS_UTILS_H

#include "types.h"

#define FS_TMP_MAX 64

extern char g_fs_tmp_paths[FS_TMP_MAX][64];
extern u8   g_fs_tmp_types[FS_TMP_MAX];
extern int  g_fs_tmp_count;

int  fs_ensure_dir(const char *path);
int  fs_ensure_file_text(const char *path, const char *text);
int  system_is_installed(void);

void resolve_path(const char *name, char *out, usize out_size);
void resolve_path_from(const char *base, const char *name, char *out, usize out_size);
const char *path_basename(const char *path);

int  path_is_self_or_child(const char *root, const char *path);
int  map_path_prefix(const char *src_root, const char *dst_root, const char *path, char *out, usize out_size);

void fs_collect_cb(const char *name, u8 is_dir, u16 size);
void fs_collect_all(void);
int  fs_dir_has_children(const char *path);

int  fs_copy_file_abs(const char *src, const char *dst);
int  fs_copy_dir_abs(const char *src, const char *dst, int recursive);
int  fs_remove_tree_abs(const char *root);

#endif
