/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Tento header definuje experimentalni API vrstvu pro C aplikace.
 * API je navrzene pro budouci user-space runtime, ale lze ho uz ted
 * pouzit pro jednotne volani kernel sluzeb v internich aplikacich.
 */

#ifndef ASTER_API_H
#define ASTER_API_H

#include "types.h"

#define ASTER_APP_API_VERSION 1

typedef struct {
    u32 api_version;
    u32 reserved;
} aster_app_context_t;

int aster_api_print(const char *text);
int aster_api_clear(void);
int aster_api_ticks(u64 *out_ticks);
void *aster_api_alloc(usize size);
int aster_api_file_create(const char *path);
int aster_api_file_read(const char *path, u8 *out, u16 max_len);
int aster_api_file_write(const char *path, const u8 *data, u16 len);
int aster_api_process_spawn(void (*entry)(void), const char *name, u8 priority);

#endif
