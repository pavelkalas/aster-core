/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento soubor deklaruje minimalisticke utility bez zavislosti na libc.
 * Obsahuje zakladni operace nad retezci a pameti, ktere kernel potrebuje
 * uz v ranych fazich bootu, kdy standardni knihovny nejsou dostupne.
 */

#ifndef ASTER_STRING_H
#define ASTER_STRING_H

#include "types.h"

usize aster_strlen(const char *s);
int aster_strcmp(const char *a, const char *b);
int aster_strncmp(const char *a, const char *b, usize n);
void *aster_memcpy(void *dst, const void *src, usize n);
void *aster_memset(void *dst, int v, usize n);

#endif
