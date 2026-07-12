/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header definuje rozhrani pametoveho subsystému jadra.
 * Obsahuje API pro allocator fyzickych stranek, jednoduche heap alokace
 * a funkce pro cteni zakladnich statistik o celkove a volne pameti.
 */

#ifndef ASTER_MEMORY_H
#define ASTER_MEMORY_H

#include "types.h"

#define PAGE_SIZE 4096

void memory_init(void);
void *page_alloc(void);
void page_free(void *page);
void *kmalloc(usize size);
void kfree(void *ptr);
usize memory_total_pages(void);
usize memory_free_pages(void);

#endif
