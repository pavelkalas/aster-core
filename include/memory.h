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

/*
 * Page table funkce pro ring 3.
 */

/** Bázová adresa PML4 tabulky (nastavená bootloaderem). */
#define PML4_BASE  0x00070000ULL
#define PDPT_BASE  0x00071000ULL
#define PD_BASE    0x00072000ULL

/** Page table flagy. */
#define PAGE_PRESENT   (1ULL << 0)
#define PAGE_WRITABLE  (1ULL << 1)
#define PAGE_USER      (1ULL << 2)
#define PAGE_HUGE      (1ULL << 7)

/**
 * Nastaví USER bit na PD entry pro danou 2MB stránku.
 * Tím zpřístupní stránku z ring 3.
 *
 * @param virtual_addr Virtuální adresa v rámci 2MB stránky (u64)
 */
void page_make_user(u64 virtual_addr);

/**
 * Alokuje fyzické stránky a namapuje je jako user-accessible.
 * Používá 2MB stránky v existující PD tabulce.
 *
 * @param virtual_addr Virtuální adresa (musí být zarovnaná na 2MB) (u64)
 */
void page_map_user_2mb(u64 virtual_addr);

#endif
