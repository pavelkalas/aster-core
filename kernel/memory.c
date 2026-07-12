/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento modul realizuje dve vrstvy spravy pameti jadra.
 * Prvni cast je bitmap allocator fyzickych stranek pro page_alloc/page_free,
 * druha cast je jednoducha linearni heap alokace pro kernelove objekty.
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

static void bitmap_set(usize idx) {
    page_bitmap[idx / 8] |= (u8)(1u << (idx % 8));
}

static void bitmap_clear(usize idx) {
    page_bitmap[idx / 8] &= (u8)~(1u << (idx % 8));
}

static int bitmap_test(usize idx) {
    return (page_bitmap[idx / 8] & (u8)(1u << (idx % 8))) != 0;
}

void memory_init(void) {
    aster_memset(page_bitmap, 0, PMM_BITMAP_SIZE);
    free_pages = PMM_MAX_PAGES;
    heap_offset = 0;

    for (usize i = 0; i < 256; ++i) {
        bitmap_set(i);
        --free_pages;
    }
}

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

void kfree(void *ptr) {
    (void)ptr;
}

usize memory_total_pages(void) {
    return PMM_MAX_PAGES;
}

usize memory_free_pages(void) {
    return free_pages;
}
