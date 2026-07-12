/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Tento modul implementuje AsterFS nad realnym ATA diskem v QEMU.
 * Data se nahravaji pri startu a po kazde zmene se synchronizuji
 * do sektoru na druhem IDE disku, aby pretrvala mezi restarty.
 */

#include "storage.h"
#include "string.h"

#define ATA_IO_BASE        0x1F0
#define ATA_CTRL_BASE      0x3F6
#define ATA_DRIVE_SLAVE    0xF0

#define ATA_REG_DATA       0
#define ATA_REG_SECCOUNT0  2
#define ATA_REG_LBA0       3
#define ATA_REG_LBA1       4
#define ATA_REG_LBA2       5
#define ATA_REG_HDDEVSEL   6
#define ATA_REG_COMMAND    7
#define ATA_REG_STATUS     7

#define ATA_CMD_READ_PIO   0x20
#define ATA_CMD_WRITE_PIO  0x30
#define ATA_CMD_CACHE_FLUSH 0xE7

#define ATA_SR_BSY         0x80
#define ATA_SR_DRQ         0x08
#define ATA_SR_ERR         0x01

#define ASTERFS_MAGIC      "ASTERFS1"
#define ASTERFS_VERSION    1U

#define ASTERFS_NODE_BYTES (ASTERFS_NAME_LEN + 1 + 2 + 1 + ASTERFS_DATA_LEN)
#define ASTERFS_NODE_SECTORS (((ASTERFS_MAX_FILES * ASTERFS_NODE_BYTES) + 511) / 512)
#define ASTERFS_DISK_START_LBA 1U

typedef struct {
    char magic[8];
    u32 version;
    u32 nodes_used;
    u32 reserved;
    u8 pad[512 - 8 - 4 - 4 - 4];
} asterfs_superblock_t;

typedef struct {
    char name[ASTERFS_NAME_LEN];
    u8 is_dir;
    u16 size;
    u8 reserved;
    u8 data[ASTERFS_DATA_LEN];
} asterfs_disk_node_t;

static asterfs_node_t nodes[ASTERFS_MAX_FILES];
static int nodes_used = 0;
static int disk_ready = 0;
static u8 g_sector_buffer[512];
static u8 g_nodes_blob[ASTERFS_MAX_FILES * ASTERFS_NODE_BYTES];

static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(unsigned short port, unsigned short value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned short inw(unsigned short port) {
    unsigned short ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void ata_delay_400ns(void) {
    (void)inb(ATA_CTRL_BASE);
    (void)inb(ATA_CTRL_BASE);
    (void)inb(ATA_CTRL_BASE);
    (void)inb(ATA_CTRL_BASE);
}

static int ata_wait_not_busy(void) {
    unsigned int timeout = 200000U;
    while (timeout-- > 0) {
        unsigned char s = inb(ATA_IO_BASE + ATA_REG_STATUS);
        if (s == 0x00 || s == 0xFF) {
            return -1;
        }
        if ((s & ATA_SR_BSY) == 0) {
            return 0;
        }
    }
    return -1;
}

static int ata_wait_drq(void) {
    unsigned int timeout = 200000U;
    while (timeout-- > 0) {
        unsigned char s = inb(ATA_IO_BASE + ATA_REG_STATUS);
        if (s == 0x00 || s == 0xFF) {
            return -1;
        }
        if (s & ATA_SR_ERR) {
            return -1;
        }
        if ((s & ATA_SR_BSY) == 0 && (s & ATA_SR_DRQ) != 0) {
            return 0;
        }
    }
    return -1;
}

static int ata_select_drive_lba(u32 lba) {
    if (ata_wait_not_busy() != 0) {
        return -1;
    }

    outb(ATA_IO_BASE + ATA_REG_HDDEVSEL, (unsigned char)(ATA_DRIVE_SLAVE | ((lba >> 24) & 0x0F)));
    ata_delay_400ns();
    return 0;
}

static int ata_probe_slave(void) {
    unsigned char s;

    outb(ATA_IO_BASE + ATA_REG_HDDEVSEL, ATA_DRIVE_SLAVE);
    ata_delay_400ns();

    s = inb(ATA_IO_BASE + ATA_REG_STATUS);
    if (s == 0x00 || s == 0xFF) {
        return -1;
    }

    return ata_wait_not_busy();
}

static int ata_read_sector(u32 lba, u8 *buf512) {
    int i;

    if (!buf512) {
        return -1;
    }

    if (ata_select_drive_lba(lba) != 0) {
        return -1;
    }

    outb(ATA_IO_BASE + ATA_REG_SECCOUNT0, 1);
    outb(ATA_IO_BASE + ATA_REG_LBA0, (unsigned char)(lba & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_LBA1, (unsigned char)((lba >> 8) & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_LBA2, (unsigned char)((lba >> 16) & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    if (ata_wait_drq() != 0) {
        return -1;
    }

    for (i = 0; i < 256; ++i) {
        unsigned short w = inw(ATA_IO_BASE + ATA_REG_DATA);
        buf512[i * 2] = (u8)(w & 0xFF);
        buf512[i * 2 + 1] = (u8)((w >> 8) & 0xFF);
    }

    return 0;
}

static int ata_write_sector(u32 lba, const u8 *buf512) {
    int i;

    if (!buf512) {
        return -1;
    }

    if (ata_select_drive_lba(lba) != 0) {
        return -1;
    }

    outb(ATA_IO_BASE + ATA_REG_SECCOUNT0, 1);
    outb(ATA_IO_BASE + ATA_REG_LBA0, (unsigned char)(lba & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_LBA1, (unsigned char)((lba >> 8) & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_LBA2, (unsigned char)((lba >> 16) & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    if (ata_wait_drq() != 0) {
        return -1;
    }

    for (i = 0; i < 256; ++i) {
        unsigned short w = (unsigned short)buf512[i * 2] | ((unsigned short)buf512[i * 2 + 1] << 8);
        outw(ATA_IO_BASE + ATA_REG_DATA, w);
    }

    outb(ATA_IO_BASE + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    if (ata_wait_not_busy() != 0) {
        return -1;
    }

    return 0;
}

static void fs_reset_memory(void) {
    int i;

    for (i = 0; i < ASTERFS_MAX_FILES; ++i) {
        nodes[i].name[0] = '\0';
        nodes[i].is_dir = 0;
        nodes[i].size = 0;
    }

    nodes_used = 0;
}

static void fs_encode_nodes(u8 *out) {
    int i;

    for (i = 0; i < ASTERFS_MAX_FILES; ++i) {
        asterfs_disk_node_t d;
        usize off = (usize)i * ASTERFS_NODE_BYTES;

        aster_memset(&d, 0, sizeof(d));
        aster_memcpy(d.name, nodes[i].name, ASTERFS_NAME_LEN);
        d.is_dir = nodes[i].is_dir;
        d.size = nodes[i].size;
        aster_memcpy(d.data, nodes[i].data, ASTERFS_DATA_LEN);

        aster_memcpy(out + off, &d, ASTERFS_NODE_BYTES);
    }
}

static void fs_decode_nodes(const u8 *in) {
    int i;

    for (i = 0; i < ASTERFS_MAX_FILES; ++i) {
        asterfs_disk_node_t d;
        usize off = (usize)i * ASTERFS_NODE_BYTES;

        aster_memcpy(&d, in + off, ASTERFS_NODE_BYTES);

        aster_memset(nodes[i].name, 0, ASTERFS_NAME_LEN);
        aster_memcpy(nodes[i].name, d.name, ASTERFS_NAME_LEN);
        nodes[i].name[ASTERFS_NAME_LEN - 1] = '\0';

        if (nodes[i].name[0] != '\0' && nodes[i].name[0] != '/') {
            nodes[i].name[0] = '\0';
        }

        nodes[i].is_dir = d.is_dir;
        if (nodes[i].is_dir > 1) {
            nodes[i].is_dir = 0;
        }
        nodes[i].size = d.size;
        if (nodes[i].size > ASTERFS_DATA_LEN) {
            nodes[i].size = ASTERFS_DATA_LEN;
        }
        aster_memcpy(nodes[i].data, d.data, ASTERFS_DATA_LEN);
    }
}

static int fs_flush_disk(void) {
    asterfs_superblock_t super;
    unsigned int sector;

    if (!disk_ready) {
        return 0;
    }

    aster_memset(&super, 0, sizeof(super));
    aster_memcpy(super.magic, ASTERFS_MAGIC, 8);
    super.version = ASTERFS_VERSION;
    super.nodes_used = (u32)nodes_used;

    aster_memset(g_sector_buffer, 0, sizeof(g_sector_buffer));
    aster_memcpy(g_sector_buffer, &super, sizeof(super));
    if (ata_write_sector(ASTERFS_DISK_START_LBA, g_sector_buffer) != 0) {
        return -1;
    }

    fs_encode_nodes(g_nodes_blob);
    for (sector = 0; sector < ASTERFS_NODE_SECTORS; ++sector) {
        usize off = (usize)sector * 512;
        aster_memset(g_sector_buffer, 0, sizeof(g_sector_buffer));
        if (off < sizeof(g_nodes_blob)) {
            usize left = sizeof(g_nodes_blob) - off;
            usize chunk = left < 512 ? left : 512;
            aster_memcpy(g_sector_buffer, g_nodes_blob + off, chunk);
        }

        if (ata_write_sector(ASTERFS_DISK_START_LBA + 1U + sector, g_sector_buffer) != 0) {
            return -1;
        }
    }

    return 0;
}

static int fs_load_disk(void) {
    asterfs_superblock_t super;
    unsigned int sector;

    if (!disk_ready) {
        return -1;
    }

    if (ata_read_sector(ASTERFS_DISK_START_LBA, g_sector_buffer) != 0) {
        return -1;
    }

    aster_memcpy(&super, g_sector_buffer, sizeof(super));

    if (aster_strncmp(super.magic, ASTERFS_MAGIC, 8) != 0 || super.version != ASTERFS_VERSION) {
        fs_reset_memory();
        return fs_flush_disk();
    }

    for (sector = 0; sector < ASTERFS_NODE_SECTORS; ++sector) {
        usize off = (usize)sector * 512;
        if (ata_read_sector(ASTERFS_DISK_START_LBA + 1U + sector, g_sector_buffer) != 0) {
            return -1;
        }

        if (off < sizeof(g_nodes_blob)) {
            usize left = sizeof(g_nodes_blob) - off;
            usize chunk = left < 512 ? left : 512;
            aster_memcpy(g_nodes_blob + off, g_sector_buffer, chunk);
        }
    }

    fs_decode_nodes(g_nodes_blob);
    if (super.nodes_used > ASTERFS_MAX_FILES) {
        nodes_used = ASTERFS_MAX_FILES;
    } else {
        nodes_used = (int)super.nodes_used;
    }

    return 0;
}

static int is_root(const char *path) {
    return path && path[0] == '/' && path[1] == '\0';
}

static int path_is_valid(const char *path) {
    usize len;

    if (!path) {
        return 0;
    }

    len = aster_strlen(path);
    if (len == 0 || len >= ASTERFS_NAME_LEN) {
        return 0;
    }

    return path[0] == '/';
}

static int path_parent_exists(const char *path) {
    int i;
    int last = -1;
    char parent[ASTERFS_NAME_LEN];

    if (!path || path[0] != '/') {
        return 0;
    }

    for (i = 0; path[i] != '\0'; ++i) {
        if (path[i] == '/') {
            last = i;
        }
    }

    if (last <= 0) {
        return 1;
    }

    if ((usize)last >= ASTERFS_NAME_LEN) {
        return 0;
    }

    aster_memcpy(parent, path, (usize)last);
    parent[last] = '\0';

    for (i = 0; i < nodes_used; ++i) {
        if (nodes[i].is_dir && aster_strcmp(nodes[i].name, parent) == 0) {
            return 1;
        }
    }

    return 0;
}

static int path_is_child(const char *parent, const char *path, const char **leaf_start) {
    usize parent_len;
    const char *rest;
    const char *p;

    if (!parent || !path || path[0] != '/') {
        return 0;
    }

    if (is_root(parent)) {
        if (path[0] != '/' || path[1] == '\0') {
            return 0;
        }

        rest = path + 1;
        p = rest;
        while (*p && *p != '/') {
            ++p;
        }

        if (*p != '\0') {
            return 0;
        }

        *leaf_start = rest;
        return 1;
    }

    parent_len = aster_strlen(parent);
    if (aster_strncmp(path, parent, parent_len) != 0) {
        return 0;
    }

    if (path[parent_len] != '/') {
        return 0;
    }

    rest = path + parent_len + 1;
    if (*rest == '\0') {
        return 0;
    }

    p = rest;
    while (*p && *p != '/') {
        ++p;
    }

    if (*p != '\0') {
        return 0;
    }

    *leaf_start = rest;
    return 1;
}

void storage_init(void) {
    asterfs_init();
}

void asterfs_init(void) {
    fs_reset_memory();

    disk_ready = (ata_probe_slave() == 0);
    if (!disk_ready) {
        return;
    }

    if (fs_load_disk() != 0) {
        fs_reset_memory();
        (void)fs_flush_disk();
    }
}

static int find_node(const char *name) {
    int i;

    if (!path_is_valid(name)) {
        return -1;
    }

    for (i = 0; i < nodes_used; ++i) {
        if (!path_is_valid(nodes[i].name)) {
            continue;
        }

        if (aster_strcmp(nodes[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

int asterfs_create_file(const char *name) {
    usize len;

    if (!path_is_valid(name) || is_root(name) || nodes_used >= ASTERFS_MAX_FILES) {
        return -1;
    }

    if (!path_parent_exists(name)) {
        return -1;
    }

    if (find_node(name) >= 0) {
        return 0;
    }

    len = aster_strlen(name);
    if (len >= ASTERFS_NAME_LEN) {
        return -1;
    }

    aster_memset(nodes[nodes_used].name, 0, ASTERFS_NAME_LEN);
    aster_memcpy(nodes[nodes_used].name, name, len);
    nodes[nodes_used].size = 0;
    nodes[nodes_used].is_dir = 0;
    ++nodes_used;
    (void)fs_flush_disk();
    return 0;
}

int asterfs_create_dir(const char *name) {
    usize len;

    if (!path_is_valid(name) || is_root(name) || nodes_used >= ASTERFS_MAX_FILES) {
        return -1;
    }

    if (!path_parent_exists(name)) {
        return -1;
    }

    if (find_node(name) >= 0) {
        return 0;
    }

    len = aster_strlen(name);
    if (len >= ASTERFS_NAME_LEN) {
        return -1;
    }

    aster_memset(nodes[nodes_used].name, 0, ASTERFS_NAME_LEN);
    aster_memcpy(nodes[nodes_used].name, name, len);
    nodes[nodes_used].size = 0;
    nodes[nodes_used].is_dir = 1;
    ++nodes_used;
    (void)fs_flush_disk();
    return 0;
}

int asterfs_remove_file(const char *name) {
    int idx;
    int i;

    if (!path_is_valid(name)) {
        return -1;
    }

    idx = find_node(name);
    if (idx < 0 || nodes[idx].is_dir) {
        return -1;
    }

    for (i = idx; i < nodes_used - 1; ++i) {
        nodes[i] = nodes[i + 1];
    }

    --nodes_used;
    (void)fs_flush_disk();
    return 0;
}

int asterfs_remove_dir(const char *name) {
    int idx;
    int i;
    const char *leaf;

    if (!path_is_valid(name) || is_root(name)) {
        return -1;
    }

    idx = find_node(name);
    if (idx < 0 || !nodes[idx].is_dir) {
        return -1;
    }

    for (i = 0; i < nodes_used; ++i) {
        if (i == idx) {
            continue;
        }

        if (path_is_child(name, nodes[i].name, &leaf)) {
            return -1;
        }
    }

    for (i = idx; i < nodes_used - 1; ++i) {
        nodes[i] = nodes[i + 1];
    }

    --nodes_used;
    (void)fs_flush_disk();
    return 0;
}

int asterfs_write_file(const char *name, const u8 *data, u16 len) {
    int idx;

    if (!path_is_valid(name) || !data || is_root(name)) {
        return -1;
    }

    idx = find_node(name);
    if (idx < 0) {
        if (asterfs_create_file(name) != 0) {
            return -1;
        }
        idx = find_node(name);
    }

    if (idx < 0 || nodes[idx].is_dir) {
        return -1;
    }

    if (len > ASTERFS_DATA_LEN) {
        len = ASTERFS_DATA_LEN;
    }

    aster_memset(nodes[idx].data, 0, ASTERFS_DATA_LEN);
    aster_memcpy(nodes[idx].data, data, len);
    nodes[idx].size = len;
    (void)fs_flush_disk();
    return len;
}

int asterfs_read_file(const char *name, u8 *out, u16 max_len) {
    int idx;
    u16 len;

    if (!path_is_valid(name) || !out || is_root(name)) {
        return -1;
    }

    idx = find_node(name);
    if (idx < 0 || nodes[idx].is_dir) {
        return -1;
    }

    len = nodes[idx].size;
    if (len > max_len) {
        len = max_len;
    }

    aster_memcpy(out, nodes[idx].data, len);
    return len;
}

void asterfs_list(void (*cb)(const char *name, u8 is_dir, u16 size)) {
    int i;

    if (!cb) {
        return;
    }

    for (i = 0; i < nodes_used; ++i) {
        if (!path_is_valid(nodes[i].name)) {
            continue;
        }

        cb(nodes[i].name, nodes[i].is_dir, nodes[i].size);
    }
}

int asterfs_get_type(const char *name) {
    int idx;

    if (is_root(name)) {
        return 1;
    }

    idx = find_node(name);
    if (idx < 0) {
        return -1;
    }

    return nodes[idx].is_dir ? 1 : 0;
}

void asterfs_list_dir(const char *path, void (*cb)(const char *name, u8 is_dir, u16 size)) {
    int i;
    const char *leaf = 0;
    char temp[ASTERFS_NAME_LEN];
    usize n;

    if (!path || !cb) {
        return;
    }

    if (!is_root(path) && !path_is_valid(path)) {
        return;
    }

    for (i = 0; i < nodes_used; ++i) {
        if (!path_is_valid(nodes[i].name)) {
            continue;
        }

        if (!path_is_child(path, nodes[i].name, &leaf)) {
            continue;
        }

        n = aster_strlen(leaf);
        if (n >= ASTERFS_NAME_LEN) {
            n = ASTERFS_NAME_LEN - 1;
        }

        aster_memcpy(temp, leaf, n);
        temp[n] = '\0';
        cb(temp, nodes[i].is_dir, nodes[i].size);
    }
}
