/*
 * AsterOS Kernel
 * Serial (COM1) diagnostics output
 */

#ifndef ASTER_SERIAL_H
#define ASTER_SERIAL_H

#include "types.h"

void serial_init(void);
int serial_is_ready(void);
void serial_write_char(char c);
void serial_write_string(const char *text);

#endif
