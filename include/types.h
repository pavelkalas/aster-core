/*
 * AsterOS Kernel
 * Autor: Pavel Kalaš
 * Rok: 2026
 *
 */

/*
 * Tento header sjednocuje základní celočíselné typy jádra.
 * Cílem je mít stabilní velikosti typů nezávisle na kompilátoru
 * a používat je konzistentně ve všech kernel subsystémech.
 */

#ifndef ASTER_TYPES_H
#define ASTER_TYPES_H

/** 64bitový unsigned integer. */
typedef unsigned long long u64;
/** 64bitový signed integer. */
typedef signed long long i64;
/** 32bitový unsigned integer. */
typedef unsigned int u32;
/** 32bitový signed integer. */
typedef signed int i32;
/** 16bitový unsigned integer. */
typedef unsigned short u16;
/** 16bitový signed integer. */
typedef signed short i16;
/** 8bitový unsigned integer. */
typedef unsigned char u8;
/** 8bitový signed integer. */
typedef signed char i8;
/** Nativní unsigned size (pointer-sized). */
typedef unsigned long usize;
/** Nativní signed size (pointer-sized). */
typedef long isize;

#endif
