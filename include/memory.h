/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header definuje rozhraní paměťového subsystému jádra.
 * Obsahuje API pro allocator fyzických stránek, jednoduché heap alokace
 * a funkce pro čtení základních statistik o celkové a volné paměti.
 */

#ifndef ASTER_MEMORY_H
#define ASTER_MEMORY_H

#include "types.h"

/** Velikost jedné stránky v bajtech. */
#define PAGE_SIZE 4096

/** Inicializuje správce paměti. */
void memory_init(void);

/** Alokuje jednu fyzickou stránku (4 KiB). */
void *page_alloc(void);

/** Uvolní dříve alokovanou fyzickou stránku. */
void page_free(void *page);

/** Alokuje paměť na kernelové haldě. */
void *kmalloc(usize size);

/** Uvolní paměť z kernelové haldy (v této implementaci nedělá nic). */
void kfree(void *ptr);

/** Vrátí celkový počet fyzických stránek. */
usize memory_total_pages(void);

/** Vrátí počet volných fyzických stránek. */
usize memory_free_pages(void);

#endif
