/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header sjednocuje zakladni celoctiselne typy jadra.
 * Cilem je mit stabilni velikosti typu nezavisle na kompilatoru
 * a pouzivat je konzistentne ve vsech kernel subsystémech.
 */

#ifndef ASTER_TYPES_H
#define ASTER_TYPES_H

typedef unsigned long long u64;
typedef signed long long i64;
typedef unsigned int u32;
typedef signed int i32;
typedef unsigned short u16;
typedef signed short i16;
typedef unsigned char u8;
typedef signed char i8;
typedef unsigned long usize;
typedef long isize;

#endif
