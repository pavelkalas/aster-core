/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Bootlog — jednotne stavove vypisy, boot stepy a shutdown priprava.
 * Pouziva se v main.c, drivers, pripadne dalsich castech jadra.
 */

#ifndef ASTER_BOOTLOG_H
#define ASTER_BOOTLOG_H

#include "types.h"

usize append_text(char *dst, usize pos, usize max, const char *src);
usize append_uint(char *dst, usize pos, usize max, unsigned int v);
int  boot_splash_is_active(void);
void boot_splash_reset(unsigned int steps_total);
void boot_splash_finish(void);
void bootlog_state(const char *state, unsigned char state_color, const char *msg);
void bootlog_ok(const char *msg);
void bootlog_error(const char *msg);
void boot_step_begin(const char *label);
void boot_step_finish(const char *label, const char *state, u8 state_color);
void boot_step_ok(const char *label);
void boot_step_skip(const char *label);

void system_shutdown_prepare(const char *typ);

#endif
