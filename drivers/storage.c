/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 * AsterFS v2 uklada data souboru jako retezy clusteru
 * (1 cluster = 1 sektor = 512 B) nad ATA slave diskem.
 *
 * Hlavni vlastnosti:
 * - soubory mohou byt vetsi nez jeden sektor,
 * - jednoducha FAT-like alokace s perzistenci na disk,
 * - zachovane API asterfs_* pro shell a sysapps.
 */

#include "storage.h"
#include "string.h"

#define ATA_IO_BASE         0x1F0
#define ATA_CTRL_BASE       0x3F6
#define ATA_DRIVE_SLAVE     0xF0

#define ATA_REG_DATA        0
#define ATA_REG_SECCOUNT0   2
#define ATA_REG_LBA0        3
#define ATA_REG_LBA1        4
#define ATA_REG_LBA2        5
#define ATA_REG_HDDEVSEL    6
#define ATA_REG_COMMAND     7
#define ATA_REG_STATUS      7

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_CACHE_FLUSH 0xE7

#define ATA_SR_BSY          0x80
#define ATA_SR_DRQ          0x08
#define ATA_SR_ERR          0x01

#define ASTERFS_MAGIC            "ASTERL2"
#define ASTERFS_VERSION          2U
#define ASTERFS_DISK_START_LBA   1U
#define ASTERFS_CLUSTER_SECTOR   512U
#define ASTERFS_CLUSTER_COUNT    4096U

#define ASTERFS_FAT_FREE         0x0000U
#define ASTERFS_FAT_EOC          0xFFFFU

/** @brief Reprezentace uzlu drzena v RAM. */
typedef struct {
    char name[ASTERFS_NAME_LEN];
    u8 is_dir;
    u16 size;
    u16 first_cluster;
} asterfs_node_mem_t;

/** @brief Superblok ulozeny na disku (presne 512 B). */
typedef struct {
    char magic[8];
    u32 version;
    u32 nodes_used;
    u32 cluster_count;
    u32 fat_start_lba;
    u32 fat_sectors;
    u32 node_start_lba;
    u32 node_sectors;
    u32 data_start_lba;
    u8 pad[512 - 8 - (8 * 4)];
} __attribute__((packed)) asterfs_superblock_t;

/** @brief Diskova serializace jednoho uzlu filesystemu. */
typedef struct {
    char name[ASTERFS_NAME_LEN];
    u8 is_dir;
    u16 size;
    u16 first_cluster;
    u8 reserved;
} __attribute__((packed)) asterfs_disk_node_t;

#define ASTERFS_NODE_BYTES    ((usize)sizeof(asterfs_disk_node_t))
#define ASTERFS_NODE_SECTORS  (((ASTERFS_MAX_FILES * ASTERFS_NODE_BYTES) + 511U) / 512U)
#define ASTERFS_FAT_BYTES     ((usize)(ASTERFS_CLUSTER_COUNT * sizeof(u16)))
#define ASTERFS_FAT_SECTORS   ((u32)((ASTERFS_FAT_BYTES + 511U) / 512U))
#define ASTERFS_FAT_START_LBA (ASTERFS_DISK_START_LBA + 1U)
#define ASTERFS_NODE_START_LBA (ASTERFS_FAT_START_LBA + ASTERFS_FAT_SECTORS)
#define ASTERFS_DATA_START_LBA (ASTERFS_NODE_START_LBA + ASTERFS_NODE_SECTORS)

_Static_assert(sizeof(asterfs_superblock_t) == 512, "asterfs_superblock_t must be 512 bytes");
_Static_assert(sizeof(asterfs_disk_node_t) == (ASTERFS_NAME_LEN + 1 + 2 + 2 + 1), "asterfs_disk_node_t layout mismatch");

/** @brief Tabulka uzlu v RAM. */
static asterfs_node_mem_t nodes[ASTERFS_MAX_FILES];
/** @brief Pocet aktivnich uzlu v tabulce @ref nodes. */
static int nodes_used = 0;
/** @brief Flag dostupnosti ATA slave disku. */
static int disk_ready = 0;
/** @brief Sdileny sektorovy buffer pro I/O operace (512 B). */
static u8 g_sector_buffer[512];
/** @brief Binarni blob FAT tabulky pro cteni/zapis vice sektoru. */
static u8 g_fat_blob[ASTERFS_FAT_BYTES];
/** @brief Binarni blob serializovanych uzlu. */
static u8 g_nodes_blob[ASTERFS_MAX_FILES * ASTERFS_NODE_BYTES];
/** @brief FAT tabulka v RAM (index -> dalsi cluster / EOC / FREE). */
static u16 g_fat[ASTERFS_CLUSTER_COUNT];
/** @brief Hint pro dalsi hledani volneho clusteru. */
static u16 g_alloc_hint = 0;

/** Zapise bajt na I/O port. */
static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/** Precte bajt z I/O portu. */
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/** Zapise 16bit hodnotu na I/O port. */
static inline void outw(unsigned short port, unsigned short value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

/** Precte 16bit hodnotu z I/O portu. */
static inline unsigned short inw(unsigned short port) {
    unsigned short ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/** Kratke zpozdeni 400 ns (4x cteni z ridiciho portu). */
static void ata_delay_400ns(void) {
    (void)inb(ATA_CTRL_BASE);
    (void)inb(ATA_CTRL_BASE);
    (void)inb(ATA_CTRL_BASE);
    (void)inb(ATA_CTRL_BASE);
}

/** Ceka, dokud neni ATA zarizeni pripraveno (BSY = 0). */
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

/** Ceka, dokud neni pripraven DRQ a neni hlasena chyba. */
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

/** Vybere ATA slave disk a nastavi LBA. */
static int ata_select_drive_lba(u32 lba) {
    if (ata_wait_not_busy() != 0) {
        return -1;
    }

    outb(ATA_IO_BASE + ATA_REG_HDDEVSEL, (unsigned char)(ATA_DRIVE_SLAVE | ((lba >> 24) & 0x0F)));
    ata_delay_400ns();
    return 0;
}

/** Otestuje, zda je slave ATA disk pritomen. */
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

/** Precte jeden sektor (512 B) z ATA disku. */
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

/** Zapise jeden sektor (512 B) na ATA disk. */
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

/** Precte vice sektoru. */
static int ata_read_sectors(u32 start_lba, u8 *buf, u32 sectors) {
    u32 i;
    for (i = 0; i < sectors; ++i) {
        if (ata_read_sector(start_lba + i, buf + (usize)i * 512U) != 0) {
            return -1;
        }
    }
    return 0;
}

/** Zapise vice sektoru. */
static int ata_write_sectors(u32 start_lba, const u8 *buf, u32 sectors) {
    u32 i;
    for (i = 0; i < sectors; ++i) {
        if (ata_write_sector(start_lba + i, buf + (usize)i * 512U) != 0) {
            return -1;
        }
    }
    return 0;
}

/** Vrati, zda je cesta root "/". */
static int is_root(const char *path) {
    return path && path[0] == '/' && path[1] == '\0';
}

/** Overi, ze cesta je platna (zacina '/' a ma spravnou delku). */
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

/** Najde uzel podle absolutni cesty. */
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

/** Vrati 1, pokud parent/path tvori primy vztah rodic-potomek. */
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

/** Zjisti, zda nadrazeny adresar cesty existuje. */
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

/** Prevede index clusteru na LBA. */
static u32 cluster_to_lba(u16 cluster) {
    return ASTERFS_DATA_START_LBA + (u32)cluster;
}

/** Vynuluje metadata FS v RAM. */
static void fs_reset_memory(void) {
    int i;

    for (i = 0; i < ASTERFS_MAX_FILES; ++i) {
        nodes[i].name[0] = '\0';
        nodes[i].is_dir = 0;
        nodes[i].size = 0;
        nodes[i].first_cluster = 0;
    }

    for (i = 0; i < (int)ASTERFS_CLUSTER_COUNT; ++i) {
        g_fat[i] = ASTERFS_FAT_FREE;
    }

    nodes_used = 0;
    g_alloc_hint = 0;
}

/** Zakoduje FAT tabulku do binarniho blobu. */
static void fs_encode_fat(void) {
    aster_memcpy(g_fat_blob, g_fat, sizeof(g_fat_blob));
}

/** Dekoduje FAT tabulku z binarniho blobu. */
static void fs_decode_fat(void) {
    aster_memcpy(g_fat, g_fat_blob, sizeof(g_fat));
}

/** Zakoduje tabulku uzlu do binarniho blobu. */
static void fs_encode_nodes(void) {
    int i;

    for (i = 0; i < ASTERFS_MAX_FILES; ++i) {
        asterfs_disk_node_t d;
        usize off = (usize)i * ASTERFS_NODE_BYTES;

        if (off + ASTERFS_NODE_BYTES > sizeof(g_nodes_blob)) {
            break;
        }

        aster_memset(&d, 0, sizeof(d));
        aster_memcpy(d.name, nodes[i].name, ASTERFS_NAME_LEN);
        d.is_dir = nodes[i].is_dir;
        d.size = nodes[i].size;
        d.first_cluster = nodes[i].first_cluster;

        aster_memcpy(g_nodes_blob + off, &d, ASTERFS_NODE_BYTES);
    }
}

/** Dekoduje tabulku uzlu z binarniho blobu. */
static void fs_decode_nodes(void) {
    int i;

    nodes_used = 0;
    for (i = 0; i < ASTERFS_MAX_FILES; ++i) {
        asterfs_disk_node_t d;
        usize off = (usize)i * ASTERFS_NODE_BYTES;

        if (off + ASTERFS_NODE_BYTES > sizeof(g_nodes_blob)) {
            break;
        }

        aster_memcpy(&d, g_nodes_blob + off, ASTERFS_NODE_BYTES);

        aster_memset(nodes[i].name, 0, ASTERFS_NAME_LEN);
        aster_memcpy(nodes[i].name, d.name, ASTERFS_NAME_LEN);
        nodes[i].name[ASTERFS_NAME_LEN - 1] = '\0';

        if (nodes[i].name[0] != '\0' && nodes[i].name[0] != '/') {
            nodes[i].name[0] = '\0';
        }

        nodes[i].is_dir = d.is_dir ? 1 : 0;
        nodes[i].size = d.size;
        nodes[i].first_cluster = d.first_cluster;

        if (!nodes[i].is_dir && nodes[i].size > 0 && nodes[i].first_cluster >= ASTERFS_CLUSTER_COUNT) {
            nodes[i].name[0] = '\0';
            nodes[i].size = 0;
            nodes[i].first_cluster = 0;
        }

        if (nodes[i].name[0] != '\0') {
            ++nodes_used;
        }
    }
}

/** Zapise superblok. */
static int fs_write_superblock(void) {
    asterfs_superblock_t super;

    aster_memset(&super, 0, sizeof(super));
    aster_memcpy(super.magic, ASTERFS_MAGIC, 8);
    super.version = ASTERFS_VERSION;
    super.nodes_used = (u32)nodes_used;
    super.cluster_count = ASTERFS_CLUSTER_COUNT;
    super.fat_start_lba = ASTERFS_FAT_START_LBA;
    super.fat_sectors = ASTERFS_FAT_SECTORS;
    super.node_start_lba = ASTERFS_NODE_START_LBA;
    super.node_sectors = ASTERFS_NODE_SECTORS;
    super.data_start_lba = ASTERFS_DATA_START_LBA;

    aster_memset(g_sector_buffer, 0, sizeof(g_sector_buffer));
    aster_memcpy(g_sector_buffer, &super, sizeof(super));
    return ata_write_sector(ASTERFS_DISK_START_LBA, g_sector_buffer);
}

/** Nacte superblok. */
static int fs_read_superblock(asterfs_superblock_t *out_super) {
    if (!out_super) {
        return -1;
    }

    if (ata_read_sector(ASTERFS_DISK_START_LBA, g_sector_buffer) != 0) {
        return -1;
    }

    aster_memcpy(out_super, g_sector_buffer, sizeof(*out_super));
    return 0;
}

/** Overi, ze superblok odpovida aktualnimu formatu. */
static int fs_superblock_is_valid(const asterfs_superblock_t *s) {
    if (!s) {
        return 0;
    }

    if (aster_strncmp(s->magic, ASTERFS_MAGIC, 8) != 0) {
        return 0;
    }

    if (s->version != ASTERFS_VERSION || s->cluster_count != ASTERFS_CLUSTER_COUNT) {
        return 0;
    }

    if (s->fat_start_lba != ASTERFS_FAT_START_LBA || s->fat_sectors != ASTERFS_FAT_SECTORS) {
        return 0;
    }

    if (s->node_start_lba != ASTERFS_NODE_START_LBA || s->node_sectors != ASTERFS_NODE_SECTORS) {
        return 0;
    }

    if (s->data_start_lba != ASTERFS_DATA_START_LBA) {
        return 0;
    }

    return 1;
}

/** Zapise metadata FS na disk. */
static int fs_flush_disk(void) {
    if (!disk_ready) {
        return 0;
    }

    if (fs_write_superblock() != 0) {
        return -1;
    }

    fs_encode_fat();
    if (ata_write_sectors(ASTERFS_FAT_START_LBA, g_fat_blob, ASTERFS_FAT_SECTORS) != 0) {
        return -1;
    }

    fs_encode_nodes();
    if (ata_write_sectors(ASTERFS_NODE_START_LBA, g_nodes_blob, ASTERFS_NODE_SECTORS) != 0) {
        return -1;
    }

    return 0;
}

/** Nacte metadata FS z disku. */
static int fs_load_disk(void) {
    asterfs_superblock_t super;

    if (!disk_ready) {
        return -1;
    }

    if (fs_read_superblock(&super) != 0) {
        return -1;
    }

    if (!fs_superblock_is_valid(&super)) {
        return -1;
    }

    if (ata_read_sectors(ASTERFS_FAT_START_LBA, g_fat_blob, ASTERFS_FAT_SECTORS) != 0) {
        return -1;
    }
    fs_decode_fat();

    if (ata_read_sectors(ASTERFS_NODE_START_LBA, g_nodes_blob, ASTERFS_NODE_SECTORS) != 0) {
        return -1;
    }
    fs_decode_nodes();

    if (super.nodes_used < (u32)nodes_used) {
        nodes_used = (int)super.nodes_used;
    }

    return 0;
}

/** Najde volny cluster. */
static int fs_find_free_cluster(void) {
    u32 i;
    u32 start = g_alloc_hint;

    for (i = 0; i < ASTERFS_CLUSTER_COUNT; ++i) {
        u32 idx = (start + i) % ASTERFS_CLUSTER_COUNT;
        if (g_fat[idx] == ASTERFS_FAT_FREE) {
            g_alloc_hint = (u16)((idx + 1U) % ASTERFS_CLUSTER_COUNT);
            return (int)idx;
        }
    }

    return -1;
}

/** Uvolni cely clusterovy retezec. */
static void fs_free_chain(u16 first_cluster) {
    u16 cur = first_cluster;
    unsigned int guard = 0;

    while (cur != ASTERFS_FAT_EOC && cur < ASTERFS_CLUSTER_COUNT && guard < ASTERFS_CLUSTER_COUNT) {
        u16 next = g_fat[cur];
        g_fat[cur] = ASTERFS_FAT_FREE;
        if (next == ASTERFS_FAT_EOC) {
            break;
        }
        cur = next;
        ++guard;
    }
}

/** Alokuje retezec o danem poctu clusteru. */
static int fs_allocate_chain(u16 needed_clusters, u16 *out_first) {
    u16 first = ASTERFS_FAT_EOC;
    u16 prev = ASTERFS_FAT_EOC;
    u16 i;

    if (!out_first) {
        return -1;
    }

    if (needed_clusters == 0) {
        *out_first = 0;
        return 0;
    }

    for (i = 0; i < needed_clusters; ++i) {
        int idx = fs_find_free_cluster();
        if (idx < 0) {
            if (first != ASTERFS_FAT_EOC) {
                fs_free_chain(first);
            }
            return -1;
        }

        g_fat[idx] = ASTERFS_FAT_EOC;
        if (prev != ASTERFS_FAT_EOC) {
            g_fat[prev] = (u16)idx;
        } else {
            first = (u16)idx;
        }
        prev = (u16)idx;
    }

    *out_first = first;
    return 0;
}

/** Zapise data do clusteroveho retezce. */
static int fs_write_chain(u16 first_cluster, const u8 *data, u16 len) {
    u16 remaining = len;
    u16 cur = first_cluster;
    unsigned int guard = 0;

    while (remaining > 0) {
        u16 chunk;

        if (cur >= ASTERFS_CLUSTER_COUNT || guard >= ASTERFS_CLUSTER_COUNT) {
            return -1;
        }

        chunk = remaining > 512U ? 512U : remaining;
        aster_memset(g_sector_buffer, 0, sizeof(g_sector_buffer));
        aster_memcpy(g_sector_buffer, data, chunk);

        if (ata_write_sector(cluster_to_lba(cur), g_sector_buffer) != 0) {
            return -1;
        }

        data += chunk;
        remaining = (u16)(remaining - chunk);

        if (remaining == 0) {
            break;
        }

        if (g_fat[cur] == ASTERFS_FAT_EOC) {
            return -1;
        }

        cur = g_fat[cur];
        ++guard;
    }

    return 0;
}

/** Precte data z clusteroveho retezce. */
static int fs_read_chain(u16 first_cluster, u8 *out, u16 len) {
    u16 remaining = len;
    u16 cur = first_cluster;
    unsigned int guard = 0;

    while (remaining > 0) {
        u16 chunk;

        if (cur >= ASTERFS_CLUSTER_COUNT || guard >= ASTERFS_CLUSTER_COUNT) {
            return -1;
        }

        if (ata_read_sector(cluster_to_lba(cur), g_sector_buffer) != 0) {
            return -1;
        }

        chunk = remaining > 512U ? 512U : remaining;
        aster_memcpy(out, g_sector_buffer, chunk);

        out += chunk;
        remaining = (u16)(remaining - chunk);

        if (remaining == 0) {
            break;
        }

        if (g_fat[cur] == ASTERFS_FAT_EOC) {
            return -1;
        }

        cur = g_fat[cur];
        ++guard;
    }

    return 0;
}

/** Vytvori prazdny FS v2 na disku. */
static int fs_format_disk(void) {
    fs_reset_memory();
    return fs_flush_disk();
}

/**
 * @brief Inicializuje storage vrstvu.
 *
 * Verejny vstupni bod ovladace; deleguje inicializaci na @ref asterfs_init.
 */
void storage_init(void) {
    asterfs_init();
}

/**
 * @brief Inicializuje AsterFS v2.
 *
 * Provede detekci ATA slave disku a pokusi se nacist metadata filesystemu.
 * Pokud metadata nejsou validni, vytvori prazdny filesystem.
 */
void asterfs_init(void) {
    int retries = 8;

    fs_reset_memory();

    disk_ready = 0;
    while (retries-- > 0) {
        disk_ready = (ata_probe_slave() == 0);
        if (disk_ready) {
            break;
        }

        /* Pockej nez QEMU inicializuje slave disk */
        {
            volatile unsigned long i;
            for (i = 0; i < 5000000UL; ++i) {
                __asm__ volatile ("pause");
            }
        }
    }

    if (!disk_ready) {
        return;
    }

    if (fs_load_disk() != 0) {
        (void)fs_format_disk();
    }
}

/**
 * @brief Vytvori novy soubor.
 * @param name Absolutni cesta souboru (napr. /dir/file).
 * @return 0 pri uspechu, -1 pri chybe.
 */
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
    nodes[nodes_used].is_dir = 0;
    nodes[nodes_used].size = 0;
    nodes[nodes_used].first_cluster = 0;
    ++nodes_used;

    if (fs_flush_disk() != 0) {
        --nodes_used;
        nodes[nodes_used].name[0] = '\0';
        return -1;
    }

    return 0;
}

/**
 * @brief Vytvori novy adresar.
 * @param name Absolutni cesta adresare.
 * @return 0 pri uspechu, -1 pri chybe.
 */
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
    nodes[nodes_used].is_dir = 1;
    nodes[nodes_used].size = 0;
    nodes[nodes_used].first_cluster = 0;
    ++nodes_used;

    if (fs_flush_disk() != 0) {
        --nodes_used;
        nodes[nodes_used].name[0] = '\0';
        return -1;
    }

    return 0;
}

/**
 * @brief Smaze soubor.
 * @param name Absolutni cesta souboru.
 * @return 0 pri uspechu, -1 pri chybe.
 */
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

    if (nodes[idx].size > 0 && nodes[idx].first_cluster < ASTERFS_CLUSTER_COUNT) {
        fs_free_chain(nodes[idx].first_cluster);
    }

    for (i = idx; i < nodes_used - 1; ++i) {
        nodes[i] = nodes[i + 1];
    }

    --nodes_used;
    if (nodes_used >= 0 && nodes_used < ASTERFS_MAX_FILES) {
        nodes[nodes_used].name[0] = '\0';
        nodes[nodes_used].size = 0;
        nodes[nodes_used].first_cluster = 0;
        nodes[nodes_used].is_dir = 0;
    }

    if (fs_flush_disk() != 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief Smaze adresar.
 * @param name Absolutni cesta adresare.
 * @return 0 pri uspechu, -1 pri chybe.
 *
 * Adresar musi byt prazdny (nesmi obsahovat prime potomky).
 */
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
    if (nodes_used >= 0 && nodes_used < ASTERFS_MAX_FILES) {
        nodes[nodes_used].name[0] = '\0';
        nodes[nodes_used].size = 0;
        nodes[nodes_used].first_cluster = 0;
        nodes[nodes_used].is_dir = 0;
    }

    if (fs_flush_disk() != 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief Zapise data do souboru.
 * @param name Absolutni cesta souboru.
 * @param data Zdrojovy buffer s daty.
 * @param len Pocet bajtu k zapisu.
 * @return Pocet zapsanych bajtu pri uspechu, jinak -1.
 *
 * Pokud soubor neexistuje, je automaticky vytvoren.
 */
int asterfs_write_file(const char *name, const u8 *data, u16 len) {
    int idx;
    u16 needed_clusters;
    u16 old_first;
    u16 old_size;
    u16 new_first = 0;

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

    needed_clusters = (u16)((len + 511U) / 512U);

    if (fs_allocate_chain(needed_clusters, &new_first) != 0) {
        return -1;
    }

    if (len > 0 && fs_write_chain(new_first, data, len) != 0) {
        if (needed_clusters > 0) {
            fs_free_chain(new_first);
        }
        return -1;
    }

    old_first = nodes[idx].first_cluster;
    old_size = nodes[idx].size;

    nodes[idx].first_cluster = new_first;
    nodes[idx].size = len;

    if (fs_flush_disk() != 0) {
        nodes[idx].first_cluster = old_first;
        nodes[idx].size = old_size;
        if (needed_clusters > 0) {
            fs_free_chain(new_first);
        }
        return -1;
    }

    if (old_size > 0 && old_first < ASTERFS_CLUSTER_COUNT) {
        fs_free_chain(old_first);
        (void)fs_flush_disk();
    }

    return (int)len;
}

/**
 * @brief Synchronizuje metadata filesystemu na disk.
 * @return 0 pri uspechu, -1 pri chybe.
 */
int asterfs_sync(void) {
    return fs_flush_disk();
}

/**
 * @brief Precte obsah souboru do vystupniho bufferu.
 * @param name Absolutni cesta souboru.
 * @param out Vystupni buffer.
 * @param max_len Maximalni pocet bajtu, ktere lze nacist.
 * @return Pocet nactenych bajtu, nebo -1 pri chybe.
 */
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

    if (len == 0) {
        return 0;
    }

    if (nodes[idx].first_cluster >= ASTERFS_CLUSTER_COUNT) {
        return -1;
    }

    if (fs_read_chain(nodes[idx].first_cluster, out, len) != 0) {
        return -1;
    }

    return (int)len;
}

/**
 * @brief Projde vsechny uzly filesystemu.
 * @param cb Callback volany pro kazdy platny uzel.
 */
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

/**
 * @brief Ziska typ uzlu.
 * @param name Absolutni cesta uzlu.
 * @return 0 = soubor, 1 = adresar, -1 = neexistuje.
 */
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

/**
 * @brief Vypise obsah adresare.
 * @param path Absolutni cesta adresare (nebo "/" pro root).
 * @param cb Callback volany pro prime potomky.
 *
 * Callback dostava pouze prime potomky zadaneho adresare,
 * nikoliv rekurzivni seznam celeho podstromu.
 */
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