/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef MEMn_H_MODULE
#define MEMn_H_MODULE

/*-****************************************
*  Dependencies
******************************************/
#include <asm/unaligned.h>  /* get_unaligned, put_unaligned* */
#include <linux/compiler.h>  /* inline */
#include <linux/swab.h>  /* swab32, swab64 */
#include <linux/types.h>  /* size_t, ptrdiff_t */
#include "debug.h"  /* DEBUG_STATIC_ASSERT */

/*-****************************************
*  Compiler specifics
******************************************/
#define MEMn_STATIC static inline

/*-**************************************************************
*  Basic Types
*****************************************************************/
typedef uint8_t  BYTE;
typedef uint16_t U16;
typedef int16_t  S16;
typedef uint32_t U32;
typedef int32_t  S32;
typedef uint64_t U64;
typedef int64_t  S64;

/*-**************************************************************
*  Memory I/O API
*****************************************************************/
/*=== Static platform detection ===*/
MEMn_STATIC unsigned MEMn_32bits(void);
MEMn_STATIC unsigned MEMn_64bits(void);
MEMn_STATIC unsigned MEMn_isLittleEndian(void);

/*=== Native unaligned read/write ===*/
MEMn_STATIC U16 MEMn_read16(const void* memPtr);
MEMn_STATIC U32 MEMn_read32(const void* memPtr);
MEMn_STATIC U64 MEMn_read64(const void* memPtr);
MEMn_STATIC size_t MEMn_readST(const void* memPtr);

MEMn_STATIC void MEMn_write16(void* memPtr, U16 value);
MEMn_STATIC void MEMn_write32(void* memPtr, U32 value);
MEMn_STATIC void MEMn_write64(void* memPtr, U64 value);

/*=== Little endian unaligned read/write ===*/
MEMn_STATIC U16 MEMn_readLE16(const void* memPtr);
MEMn_STATIC U32 MEMn_readLE24(const void* memPtr);
MEMn_STATIC U32 MEMn_readLE32(const void* memPtr);
MEMn_STATIC U64 MEMn_readLE64(const void* memPtr);
MEMn_STATIC size_t MEMn_readLEST(const void* memPtr);

MEMn_STATIC void MEMn_writeLE16(void* memPtr, U16 val);
MEMn_STATIC void MEMn_writeLE24(void* memPtr, U32 val);
MEMn_STATIC void MEMn_writeLE32(void* memPtr, U32 val32);
MEMn_STATIC void MEMn_writeLE64(void* memPtr, U64 val64);
MEMn_STATIC void MEMn_writeLEST(void* memPtr, size_t val);

/*=== Big endian unaligned read/write ===*/
MEMn_STATIC U32 MEMn_readBE32(const void* memPtr);
MEMn_STATIC U64 MEMn_readBE64(const void* memPtr);
MEMn_STATIC size_t MEMn_readBEST(const void* memPtr);

MEMn_STATIC void MEMn_writeBE32(void* memPtr, U32 val32);
MEMn_STATIC void MEMn_writeBE64(void* memPtr, U64 val64);
MEMn_STATIC void MEMn_writeBEST(void* memPtr, size_t val);

/*=== Byteswap ===*/
MEMn_STATIC U32 MEMn_swap32(U32 in);
MEMn_STATIC U64 MEMn_swap64(U64 in);
MEMn_STATIC size_t MEMn_swapST(size_t in);

/*-**************************************************************
*  Memory I/O Implementation
*****************************************************************/
MEMn_STATIC unsigned MEMn_32bits(void)
{
    return sizeof(size_t) == 4;
}

MEMn_STATIC unsigned MEMn_64bits(void)
{
    return sizeof(size_t) == 8;
}

#if defined(__LITTLE_ENDIAN)
#define MEMn_LITTLE_ENDIAN 1
#else
#define MEMn_LITTLE_ENDIAN 0
#endif

MEMn_STATIC unsigned MEMn_isLittleEndian(void)
{
    return MEMn_LITTLE_ENDIAN;
}

MEMn_STATIC U16 MEMn_read16(const void *memPtr)
{
    return get_unaligned((const U16 *)memPtr);
}

MEMn_STATIC U32 MEMn_read32(const void *memPtr)
{
    return get_unaligned((const U32 *)memPtr);
}

MEMn_STATIC U64 MEMn_read64(const void *memPtr)
{
    return get_unaligned((const U64 *)memPtr);
}

MEMn_STATIC size_t MEMn_readST(const void *memPtr)
{
    return get_unaligned((const size_t *)memPtr);
}

MEMn_STATIC void MEMn_write16(void *memPtr, U16 value)
{
    put_unaligned(value, (U16 *)memPtr);
}

MEMn_STATIC void MEMn_write32(void *memPtr, U32 value)
{
    put_unaligned(value, (U32 *)memPtr);
}

MEMn_STATIC void MEMn_write64(void *memPtr, U64 value)
{
    put_unaligned(value, (U64 *)memPtr);
}

/*=== Little endian r/w ===*/

MEMn_STATIC U16 MEMn_readLE16(const void *memPtr)
{
    return get_unaligned_le16(memPtr);
}

MEMn_STATIC void MEMn_writeLE16(void *memPtr, U16 val)
{
    put_unaligned_le16(val, memPtr);
}

MEMn_STATIC U32 MEMn_readLE24(const void *memPtr)
{
    return MEMn_readLE16(memPtr) + (((const BYTE *)memPtr)[2] << 16);
}

MEMn_STATIC void MEMn_writeLE24(void *memPtr, U32 val)
{
	MEMn_writeLE16(memPtr, (U16)val);
	((BYTE *)memPtr)[2] = (BYTE)(val >> 16);
}

MEMn_STATIC U32 MEMn_readLE32(const void *memPtr)
{
    return get_unaligned_le32(memPtr);
}

MEMn_STATIC void MEMn_writeLE32(void *memPtr, U32 val32)
{
    put_unaligned_le32(val32, memPtr);
}

MEMn_STATIC U64 MEMn_readLE64(const void *memPtr)
{
    return get_unaligned_le64(memPtr);
}

MEMn_STATIC void MEMn_writeLE64(void *memPtr, U64 val64)
{
    put_unaligned_le64(val64, memPtr);
}

MEMn_STATIC size_t MEMn_readLEST(const void *memPtr)
{
	if (MEMn_32bits())
		return (size_t)MEMn_readLE32(memPtr);
	else
		return (size_t)MEMn_readLE64(memPtr);
}

MEMn_STATIC void MEMn_writeLEST(void *memPtr, size_t val)
{
	if (MEMn_32bits())
		MEMn_writeLE32(memPtr, (U32)val);
	else
		MEMn_writeLE64(memPtr, (U64)val);
}

/*=== Big endian r/w ===*/

MEMn_STATIC U32 MEMn_readBE32(const void *memPtr)
{
    return get_unaligned_be32(memPtr);
}

MEMn_STATIC void MEMn_writeBE32(void *memPtr, U32 val32)
{
    put_unaligned_be32(val32, memPtr);
}

MEMn_STATIC U64 MEMn_readBE64(const void *memPtr)
{
    return get_unaligned_be64(memPtr);
}

MEMn_STATIC void MEMn_writeBE64(void *memPtr, U64 val64)
{
    put_unaligned_be64(val64, memPtr);
}

MEMn_STATIC size_t MEMn_readBEST(const void *memPtr)
{
	if (MEMn_32bits())
		return (size_t)MEMn_readBE32(memPtr);
	else
		return (size_t)MEMn_readBE64(memPtr);
}

MEMn_STATIC void MEMn_writeBEST(void *memPtr, size_t val)
{
	if (MEMn_32bits())
		MEMn_writeBE32(memPtr, (U32)val);
	else
		MEMn_writeBE64(memPtr, (U64)val);
}

MEMn_STATIC U32 MEMn_swap32(U32 in)
{
    return swab32(in);
}

MEMn_STATIC U64 MEMn_swap64(U64 in)
{
    return swab64(in);
}

MEMn_STATIC size_t MEMn_swapST(size_t in)
{
    if (MEMn_32bits())
        return (size_t)MEMn_swap32((U32)in);
    else
        return (size_t)MEMn_swap64((U64)in);
}

#endif /* MEMn_H_MODULE */
