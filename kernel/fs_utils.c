/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento modul implementuje FS utility pro praci se souborovym systemem.
 * Obsahuje funkce pro resolvepath, kopirovani a mazani adresarovych stromu,
 * zajisteni existence adresaru a souboru a kontrolu instalace systemu.
 */

#include "fs_utils.h"
#include "bootlog.h"
#include "printk.h"
#include "storage.h"
#include "string.h"

#define INSTALL_FLAG_FILE "/.installed"

char g_fs_tmp_paths[FS_TMP_MAX][64];
u8   g_fs_tmp_types[FS_TMP_MAX];
int  g_fs_tmp_count = 0;

int fs_ensure_dir(const char *path) {
    int t;
    if (!path || path[0] == '\0') return -1;
    t = asterfs_get_type(path);
    if (t == 1) return 0;
    if (t == 0) return -1;
    return asterfs_create_dir(path);
}

int fs_ensure_file_text(const char *path, const char *text) {
    int t;
    if (!path || !text || path[0] == '\0') return -1;
    t = asterfs_get_type(path);
    if (t == 1) return -1;
    if (t < 0 && asterfs_create_file(path) != 0) return -1;
    return asterfs_write_file(path, (const u8 *)text, (u16)aster_strlen(text)) < 0 ? -1 : 0;
}

int system_is_installed(void) {
    return asterfs_get_type(INSTALL_FLAG_FILE) == 0;
}

const char *path_basename(const char *path) {
    usize i = aster_strlen(path);
    while (i > 0) { if (path[i - 1] == '/') return &path[i]; --i; }
    return path;
}

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* Forward reference */
extern char g_cwd[64];

void resolve_path(const char *name, char *out, usize out_size) {
    usize i = 0, j = 0;
    if (!name || !out || out_size < 2) return;
    if (name[0] == '/') {
        while (name[i] && i < out_size - 1) { out[i] = name[i]; ++i; }
        out[i] = '\0'; return;
    }
    if (aster_strcmp(name, ".") == 0) { resolve_path(g_cwd, out, out_size); return; }
    if (aster_strcmp(name, "..") == 0) {
        usize n = aster_strlen(g_cwd);
        if (n <= 1) { out[0] = '/'; out[1] = '\0'; return; }
        while (n > 1 && g_cwd[n - 1] != '/') --n;
        if (n > 1) --n;
        for (i = 0; i < n && i < out_size - 1; ++i) out[i] = g_cwd[i];
        if (i == 0) out[i++] = '/';
        out[i] = '\0'; return;
    }
    if (aster_strcmp(g_cwd, "/") == 0) out[j++] = '/';
    else {
        while (g_cwd[i] && j < out_size - 1) out[j++] = g_cwd[i++];
        if (j < out_size - 1) out[j++] = '/';
    }
    i = 0;
    while (name[i] && j < out_size - 1) out[j++] = name[i++];
    out[j] = '\0';
}

void resolve_path_from(const char *base, const char *name, char *out, usize out_size) {
    usize i = 0, j = 0;
    if (!base || !name || !out || out_size < 2) return;
    if (name[0] == '/') {
        while (name[i] && i < out_size - 1) { out[i] = name[i]; ++i; }
        out[i] = '\0'; return;
    }
    if (aster_strcmp(name, ".") == 0) { resolve_path_from("/", base, out, out_size); return; }
    if (aster_strcmp(name, "..") == 0) {
        usize n = aster_strlen(base);
        if (n <= 1) { out[0] = '/'; out[1] = '\0'; return; }
        while (n > 1 && base[n - 1] != '/') --n;
        if (n > 1) --n;
        for (i = 0; i < n && i < out_size - 1; ++i) out[i] = base[i];
        if (i == 0) out[i++] = '/';
        out[i] = '\0'; return;
    }
    if (aster_strcmp(base, "/") == 0) out[j++] = '/';
    else {
        while (base[i] && j < out_size - 1) out[j++] = base[i++];
        if (j < out_size - 1) out[j++] = '/';
    }
    i = 0;
    while (name[i] && j < out_size - 1) out[j++] = name[i++];
    out[j] = '\0';
}

int path_is_self_or_child(const char *root, const char *path) {
    usize n;
    if (!root || !path) return 0;
    if (aster_strcmp(root, path) == 0) return 1;
    n = aster_strlen(root);
    if (n == 0) return 0;
    if (aster_strncmp(path, root, n) != 0) return 0;
    return path[n] == '/';
}

int map_path_prefix(const char *src_root, const char *dst_root, const char *path, char *out, usize out_size) {
    usize src_n, dst_n, suffix_n;
    if (!path_is_self_or_child(src_root, path)) return -1;
    src_n = aster_strlen(src_root);
    dst_n = aster_strlen(dst_root);
    if (aster_strcmp(src_root, path) == 0) {
        if (dst_n + 1 > out_size) return -1;
        aster_memcpy(out, dst_root, dst_n);
        out[dst_n] = '\0'; return 0;
    }
    suffix_n = aster_strlen(path + src_n);
    if (dst_n + suffix_n + 1 > out_size) return -1;
    aster_memcpy(out, dst_root, dst_n);
    aster_memcpy(out + dst_n, path + src_n, suffix_n);
    out[dst_n + suffix_n] = '\0';
    return 0;
}

void fs_collect_cb(const char *name, u8 is_dir, u16 size) {
    usize n;
    (void)size;
    if (g_fs_tmp_count >= FS_TMP_MAX) return;
    n = aster_strlen(name);
    if (n >= 64) n = 63;
    aster_memcpy(g_fs_tmp_paths[g_fs_tmp_count], name, n);
    g_fs_tmp_paths[g_fs_tmp_count][n] = '\0';
    g_fs_tmp_types[g_fs_tmp_count] = is_dir;
    ++g_fs_tmp_count;
}

void fs_collect_all(void) {
    g_fs_tmp_count = 0;
    asterfs_list(fs_collect_cb);
}

int fs_dir_has_children(const char *path) {
    int i;
    fs_collect_all();
    for (i = 0; i < g_fs_tmp_count; ++i)
        if (path_is_self_or_child(path, g_fs_tmp_paths[i]) && aster_strcmp(path, g_fs_tmp_paths[i]) != 0)
            return 1;
    return 0;
}

int fs_copy_file_abs(const char *src, const char *dst) {
    u8 buf[4096];
    int n;
    if (asterfs_get_type(src) != 0) return -1;
    n = asterfs_read_file(src, buf, 4096);
    if (n < 0) return -1;
    if (asterfs_write_file(dst, buf, (u16)n) < 0) return -1;
    return 0;
}

int fs_copy_dir_abs(const char *src, const char *dst, int recursive) {
    int i;
    char mapped[64];
    if (asterfs_get_type(src) != 1) return -1;
    if (asterfs_get_type(dst) >= 0) return -1;
    if (!recursive && fs_dir_has_children(src)) return -2;
    if (asterfs_create_dir(dst) != 0) return -1;
    if (!recursive) return 0;

    fs_collect_all();
    for (i = 0; i < g_fs_tmp_count; ++i) {
        if (!g_fs_tmp_types[i]) continue;
        if (aster_strcmp(g_fs_tmp_paths[i], src) == 0) continue;
        if (!path_is_self_or_child(src, g_fs_tmp_paths[i])) continue;
        if (map_path_prefix(src, dst, g_fs_tmp_paths[i], mapped, sizeof(mapped)) != 0) return -1;
        if (asterfs_create_dir(mapped) != 0) return -1;
    }
    for (i = 0; i < g_fs_tmp_count; ++i) {
        if (g_fs_tmp_types[i]) continue;
        if (!path_is_self_or_child(src, g_fs_tmp_paths[i])) continue;
        if (map_path_prefix(src, dst, g_fs_tmp_paths[i], mapped, sizeof(mapped)) != 0) return -1;
        if (fs_copy_file_abs(g_fs_tmp_paths[i], mapped) != 0) return -1;
    }
    return 0;
}

int fs_remove_tree_abs(const char *root) {
    int i, j;
    fs_collect_all();
    for (i = 0; i < g_fs_tmp_count; ++i) {
        if (g_fs_tmp_types[i]) continue;
        if (!path_is_self_or_child(root, g_fs_tmp_paths[i])) continue;
        if (asterfs_remove_file(g_fs_tmp_paths[i]) != 0) return -1;
    }
    for (i = 0; i < g_fs_tmp_count; ++i) {
        int best = -1;
        usize best_len = 0;
        for (j = 0; j < g_fs_tmp_count; ++j) {
            if (!g_fs_tmp_types[j] || g_fs_tmp_paths[j][0] == '\0') continue;
            if (!path_is_self_or_child(root, g_fs_tmp_paths[j])) continue;
            usize len = aster_strlen(g_fs_tmp_paths[j]);
            if (best == -1 || len > best_len) { best = j; best_len = len; }
        }
        if (best == -1) break;
        if (asterfs_remove_dir(g_fs_tmp_paths[best]) != 0) return -1;
        g_fs_tmp_paths[best][0] = '\0';
    }
    return 0;
}
