/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento modul realizuje dvě vrstvy správy paměti jádra.
 * První část je bitmap allocator fyzických stránek pro page_alloc/page_free,
 * druhá část je jednoduchá lineární heap alokace pro kernelové objekty.
 */

#include "memory.h"
#include "string.h"

#define PMM_MAX_MEMORY (64ULL * 1024ULL * 1024ULL)
#define PMM_MAX_PAGES  (PMM_MAX_MEMORY / PAGE_SIZE)
#define PMM_BITMAP_SIZE (PMM_MAX_PAGES / 8)

static u8 page_bitmap[PMM_BITMAP_SIZE];
static usize free_pages = PMM_MAX_PAGES;

#define KHEAP_SIZE (1024 * 1024)
static u8 kernel_heap[KHEAP_SIZE];
static usize heap_offset = 0;

/**
 * Nastaví bit v bitmapě stránek (označí stránku jako obsazenou).
 *
 * @param idx Index stránky (usize)
 */
static void bitmap_set(usize idx) {
    page_bitmap[idx / 8] |= (u8)(1u << (idx % 8));
}

/**
 * Vymaže bit v bitmapě stránek (označí stránku jako volnou).
 *
 * @param idx Index stránky (usize)
 */
static void bitmap_clear(usize idx) {
    page_bitmap[idx / 8] &= (u8)~(1u << (idx % 8));
}

/**
 * Otestuje, zda je stránka obsazená.
 *
 * @param idx Index stránky (usize)
 * @return    Nenulová hodnota pokud je stránka obsazená, jinak 0 (int)
 */
static int bitmap_test(usize idx) {
    return (page_bitmap[idx / 8] & (u8)(1u << (idx % 8))) != 0;
}

/**
 * Inicializuje správce paměti – vynuluje bitmapu, nastaví počet volných stránek
 * a rezervuje prvních 256 stránek (pro jádro a bootovací data).
 */
void memory_init(void) {
    aster_memset(page_bitmap, 0, PMM_BITMAP_SIZE);
    free_pages = PMM_MAX_PAGES;
    heap_offset = 0;

    for (usize i = 0; i < 256; ++i) {
        bitmap_set(i);
        --free_pages;
    }
}

/**
 * Alokuje jednu fyzickou stránku (4 KiB).
 * Prohledává bitmapu a vrací adresu první volné stránky.
 *
 * @return Ukazatel na začátek stránky, nebo NULL pokud není volná paměť (void *)
 */
void *page_alloc(void) {
    usize i;

    for (i = 0; i < PMM_MAX_PAGES; ++i) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            --free_pages;
            return (void *)(i * PAGE_SIZE);
        }
    }

    return 0;
}

/**
 * Uvolní dříve alokovanou fyzickou stránku.
 *
 * @param page Ukazatel na stránku (void *)
 */
void page_free(void *page) {
    usize idx;

    if (!page) {
        return;
    }

    idx = ((usize)page) / PAGE_SIZE;
    if (idx < PMM_MAX_PAGES && bitmap_test(idx)) {
        bitmap_clear(idx);
        ++free_pages;
    }
}

/**
 * Alokuje paměť na kernelové haldě (lineární alokace – bez dealokace).
 * Velikost je zaokrouhlena nahoru na 16 bajtů.
 *
 * @param size Požadovaná velikost v bajtech (usize)
 * @return     Ukazatel na alokovanou paměť, nebo NULL při nedostatku místa (void *)
 */
void *kmalloc(usize size) {
    usize aligned;

    if (size == 0) {
        return 0;
    }

    aligned = (size + 15) & ~((usize)15);
    if (heap_offset + aligned > KHEAP_SIZE) {
        return 0;
    }

    void *ptr = &kernel_heap[heap_offset];
    heap_offset += aligned;
    return ptr;
}

/**
 "Uvolní" paměť na kernelové haldě (v této implementaci nedělá nic).
 *
 * @param ptr Ukazatel na paměť (void *)
 */
void kfree(void *ptr) {
    (void)ptr;
}

/**
 * Vrátí celkový počet fyzických stránek.
 *
 * @return Počet stránek (usize)
 */
usize memory_total_pages(void) {
    return PMM_MAX_PAGES;
}

/**
 * Vrátí počet volných fyzických stránek.
 *
 * @return Počet volných stránek (usize)
 */
usize memory_free_pages(void) {
    return free_pages;
}
