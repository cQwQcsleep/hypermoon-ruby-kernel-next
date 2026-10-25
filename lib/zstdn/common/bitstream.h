/* ******************************************************************
 * bitstream
 * Part of FSE library
 * Copyright (c) Yann Collet, Facebook, Inc.
 *
 * You can contact the author at :
 * - Source repository : https://github.com/Cyan4973/FiniteStateEntropy
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
****************************************************************** */
#ifndef BITSTREAM_H_MODULE
#define BITSTREAM_H_MODULE

/*
*  This API consists of small unitary functions, which must be inlined for best performance.
*  Since link-time-optimization is not available for all compilers,
*  these functions are defined into a .h to be included.
*/

/*-****************************************
*  Dependencies
******************************************/
#include "mem.h"            /* unaligned access routines */
#include "compiler.h"       /* UNLIKELY() */
#include "debug.h"          /* assert(), DEBUGLOG(), RAWLOG() */
#include "error_private.h"  /* error codes and messages */


/*=========================================
*  Target specific
=========================================*/

#define STREAM_ACCUMULATOR_MIN_32  25
#define STREAM_ACCUMULATOR_MIN_64  57
#define STREAM_ACCUMULATOR_MIN    ((U32)(MEMn_32bits() ? STREAM_ACCUMULATOR_MIN_32 : STREAM_ACCUMULATOR_MIN_64))


/*-******************************************
*  bitStream encoding API (write forward)
********************************************/
/* bitStream can mix input from multiple sources.
 * A critical property of these streams is that they encode and decode in **reverse** direction.
 * So the first bit sequence you add will be the last to be read, like a LIFO stack.
 */
typedef struct {
    size_t bitContainer;
    unsigned bitPos;
    char*  startPtr;
    char*  ptr;
    char*  endPtr;
} BITn_CStream_t;

MEMn_STATIC size_t BITn_initCStream(BITn_CStream_t* bitC, void* dstBuffer, size_t dstCapacity);
MEMn_STATIC void   BITn_addBits(BITn_CStream_t* bitC, size_t value, unsigned nbBits);
MEMn_STATIC void   BITn_flushBits(BITn_CStream_t* bitC);
MEMn_STATIC size_t BITn_closeCStream(BITn_CStream_t* bitC);

/* Start with initCStream, providing the size of buffer to write into.
*  bitStream will never write outside of this buffer.
*  `dstCapacity` must be >= sizeof(bitD->bitContainer), otherwise @return will be an error code.
*
*  bits are first added to a local register.
*  Local register is size_t, hence 64-bits on 64-bits systems, or 32-bits on 32-bits systems.
*  Writing data into memory is an explicit operation, performed by the flushBits function.
*  Hence keep track how many bits are potentially stored into local register to avoid register overflow.
*  After a flushBits, a maximum of 7 bits might still be stored into local register.
*
*  Avoid storing elements of more than 24 bits if you want compatibility with 32-bits bitstream readers.
*
*  Last operation is to close the bitStream.
*  The function returns the final size of CStream in bytes.
*  If data couldn't fit into `dstBuffer`, it will return a 0 ( == not storable)
*/


/*-********************************************
*  bitStream decoding API (read backward)
**********************************************/
typedef struct {
    size_t   bitContainer;
    unsigned bitsConsumed;
    const char* ptr;
    const char* start;
    const char* limitPtr;
} BITn_DStream_t;

typedef enum { BITn_DStream_unfinished = 0,
               BITn_DStream_endOfBuffer = 1,
               BITn_DStream_completed = 2,
               BITn_DStream_overflow = 3 } BITn_DStream_status;  /* result of BITn_reloadDStream() */
               /* 1,2,4,8 would be better for bitmap combinations, but slows down performance a bit ... :( */

MEMn_STATIC size_t   BITn_initDStream(BITn_DStream_t* bitD, const void* srcBuffer, size_t srcSize);
MEMn_STATIC size_t   BITn_readBits(BITn_DStream_t* bitD, unsigned nbBits);
MEMn_STATIC BITn_DStream_status BITn_reloadDStream(BITn_DStream_t* bitD);
MEMn_STATIC unsigned BITn_endOfDStream(const BITn_DStream_t* bitD);


/* Start by invoking BITn_initDStream().
*  A chunk of the bitStream is then stored into a local register.
*  Local register size is 64-bits on 64-bits systems, 32-bits on 32-bits systems (size_t).
*  You can then retrieve bitFields stored into the local register, **in reverse order**.
*  Local register is explicitly reloaded from memory by the BITn_reloadDStream() method.
*  A reload guarantee a minimum of ((8*sizeof(bitD->bitContainer))-7) bits when its result is BITn_DStream_unfinished.
*  Otherwise, it can be less than that, so proceed accordingly.
*  Checking if DStream has reached its end can be performed with BITn_endOfDStream().
*/


/*-****************************************
*  unsafe API
******************************************/
MEMn_STATIC void BITn_addBitsFast(BITn_CStream_t* bitC, size_t value, unsigned nbBits);
/* faster, but works only if value is "clean", meaning all high bits above nbBits are 0 */

MEMn_STATIC void BITn_flushBitsFast(BITn_CStream_t* bitC);
/* unsafe version; does not check buffer overflow */

MEMn_STATIC size_t BITn_readBitsFast(BITn_DStream_t* bitD, unsigned nbBits);
/* faster, but works only if nbBits >= 1 */



/*-**************************************************************
*  Internal functions
****************************************************************/
MEMn_STATIC unsigned BITn_highbit32 (U32 val)
{
    assert(val != 0);
    {
#   if (__GNUC__ >= 3)   /* Use GCC Intrinsic */
        return __builtin_clz (val) ^ 31;
#   else   /* Software version */
        static const unsigned DeBruijnClz[32] = { 0,  9,  1, 10, 13, 21,  2, 29,
                                                 11, 14, 16, 18, 22, 25,  3, 30,
                                                  8, 12, 20, 28, 15, 17, 24,  7,
                                                 19, 27, 23,  6, 26,  5,  4, 31 };
        U32 v = val;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        return DeBruijnClz[ (U32) (v * 0x07C4ACDDU) >> 27];
#   endif
    }
}

/*=====    Local Constants   =====*/
static const unsigned BITn_mask[] = {
    0,          1,         3,         7,         0xF,       0x1F,
    0x3F,       0x7F,      0xFF,      0x1FF,     0x3FF,     0x7FF,
    0xFFF,      0x1FFF,    0x3FFF,    0x7FFF,    0xFFFF,    0x1FFFF,
    0x3FFFF,    0x7FFFF,   0xFFFFF,   0x1FFFFF,  0x3FFFFF,  0x7FFFFF,
    0xFFFFFF,   0x1FFFFFF, 0x3FFFFFF, 0x7FFFFFF, 0xFFFFFFF, 0x1FFFFFFF,
    0x3FFFFFFF, 0x7FFFFFFF}; /* up to 31 bits */
#define BITn_MASK_SIZE (sizeof(BITn_mask) / sizeof(BITn_mask[0]))

/*-**************************************************************
*  bitStream encoding
****************************************************************/
/*! BITn_initCStream() :
 *  `dstCapacity` must be > sizeof(size_t)
 *  @return : 0 if success,
 *            otherwise an error code (can be tested using ERR_isError()) */
MEMn_STATIC size_t BITn_initCStream(BITn_CStream_t* bitC,
                                  void* startPtr, size_t dstCapacity)
{
    bitC->bitContainer = 0;
    bitC->bitPos = 0;
    bitC->startPtr = (char*)startPtr;
    bitC->ptr = bitC->startPtr;
    bitC->endPtr = bitC->startPtr + dstCapacity - sizeof(bitC->bitContainer);
    if (dstCapacity <= sizeof(bitC->bitContainer)) return ERROR(dstSize_tooSmall);
    return 0;
}

/*! BITn_addBits() :
 *  can add up to 31 bits into `bitC`.
 *  Note : does not check for register overflow ! */
MEMn_STATIC void BITn_addBits(BITn_CStream_t* bitC,
                            size_t value, unsigned nbBits)
{
    DEBUG_STATIC_ASSERT(BITn_MASK_SIZE == 32);
    assert(nbBits < BITn_MASK_SIZE);
    assert(nbBits + bitC->bitPos < sizeof(bitC->bitContainer) * 8);
    bitC->bitContainer |= (value & BITn_mask[nbBits]) << bitC->bitPos;
    bitC->bitPos += nbBits;
}

/*! BITn_addBitsFast() :
 *  works only if `value` is _clean_,
 *  meaning all high bits above nbBits are 0 */
MEMn_STATIC void BITn_addBitsFast(BITn_CStream_t* bitC,
                                size_t value, unsigned nbBits)
{
    assert((value>>nbBits) == 0);
    assert(nbBits + bitC->bitPos < sizeof(bitC->bitContainer) * 8);
    bitC->bitContainer |= value << bitC->bitPos;
    bitC->bitPos += nbBits;
}

/*! BITn_flushBitsFast() :
 *  assumption : bitContainer has not overflowed
 *  unsafe version; does not check buffer overflow */
MEMn_STATIC void BITn_flushBitsFast(BITn_CStream_t* bitC)
{
    size_t const nbBytes = bitC->bitPos >> 3;
    assert(bitC->bitPos < sizeof(bitC->bitContainer) * 8);
    assert(bitC->ptr <= bitC->endPtr);
    MEMn_writeLEST(bitC->ptr, bitC->bitContainer);
    bitC->ptr += nbBytes;
    bitC->bitPos &= 7;
    bitC->bitContainer >>= nbBytes*8;
}

/*! BITn_flushBits() :
 *  assumption : bitContainer has not overflowed
 *  safe version; check for buffer overflow, and prevents it.
 *  note : does not signal buffer overflow.
 *  overflow will be revealed later on using BITn_closeCStream() */
MEMn_STATIC void BITn_flushBits(BITn_CStream_t* bitC)
{
    size_t const nbBytes = bitC->bitPos >> 3;
    assert(bitC->bitPos < sizeof(bitC->bitContainer) * 8);
    assert(bitC->ptr <= bitC->endPtr);
    MEMn_writeLEST(bitC->ptr, bitC->bitContainer);
    bitC->ptr += nbBytes;
    if (bitC->ptr > bitC->endPtr) bitC->ptr = bitC->endPtr;
    bitC->bitPos &= 7;
    bitC->bitContainer >>= nbBytes*8;
}

/*! BITn_closeCStream() :
 *  @return : size of CStream, in bytes,
 *            or 0 if it could not fit into dstBuffer */
MEMn_STATIC size_t BITn_closeCStream(BITn_CStream_t* bitC)
{
    BITn_addBitsFast(bitC, 1, 1);   /* endMark */
    BITn_flushBits(bitC);
    if (bitC->ptr >= bitC->endPtr) return 0; /* overflow detected */
    return (bitC->ptr - bitC->startPtr) + (bitC->bitPos > 0);
}


/*-********************************************************
*  bitStream decoding
**********************************************************/
/*! BITn_initDStream() :
 *  Initialize a BITn_DStream_t.
 * `bitD` : a pointer to an already allocated BITn_DStream_t structure.
 * `srcSize` must be the *exact* size of the bitStream, in bytes.
 * @return : size of stream (== srcSize), or an errorCode if a problem is detected
 */
MEMn_STATIC size_t BITn_initDStream(BITn_DStream_t* bitD, const void* srcBuffer, size_t srcSize)
{
    if (srcSize < 1) { ZSTDn_memset(bitD, 0, sizeof(*bitD)); return ERROR(srcSize_wrong); }

    bitD->start = (const char*)srcBuffer;
    bitD->limitPtr = bitD->start + sizeof(bitD->bitContainer);

    if (srcSize >=  sizeof(bitD->bitContainer)) {  /* normal case */
        bitD->ptr   = (const char*)srcBuffer + srcSize - sizeof(bitD->bitContainer);
        bitD->bitContainer = MEMn_readLEST(bitD->ptr);
        { BYTE const lastByte = ((const BYTE*)srcBuffer)[srcSize-1];
          bitD->bitsConsumed = lastByte ? 8 - BITn_highbit32(lastByte) : 0;  /* ensures bitsConsumed is always set */
          if (lastByte == 0) return ERROR(GENERIC); /* endMark not present */ }
    } else {
        bitD->ptr   = bitD->start;
        bitD->bitContainer = *(const BYTE*)(bitD->start);
        switch(srcSize)
        {
        case 7: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[6]) << (sizeof(bitD->bitContainer)*8 - 16);
                ZSTDn_FALLTHROUGH;

        case 6: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[5]) << (sizeof(bitD->bitContainer)*8 - 24);
                ZSTDn_FALLTHROUGH;

        case 5: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[4]) << (sizeof(bitD->bitContainer)*8 - 32);
                ZSTDn_FALLTHROUGH;

        case 4: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[3]) << 24;
                ZSTDn_FALLTHROUGH;

        case 3: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[2]) << 16;
                ZSTDn_FALLTHROUGH;

        case 2: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[1]) <<  8;
                ZSTDn_FALLTHROUGH;

        default: break;
        }
        {   BYTE const lastByte = ((const BYTE*)srcBuffer)[srcSize-1];
            bitD->bitsConsumed = lastByte ? 8 - BITn_highbit32(lastByte) : 0;
            if (lastByte == 0) return ERROR(corruption_detected);  /* endMark not present */
        }
        bitD->bitsConsumed += (U32)(sizeof(bitD->bitContainer) - srcSize)*8;
    }

    return srcSize;
}

MEMn_STATIC FORCE_INLINE_ATTR size_t BITn_getUpperBits(size_t bitContainer, U32 const start)
{
    return bitContainer >> start;
}

MEMn_STATIC FORCE_INLINE_ATTR size_t BITn_getMiddleBits(size_t bitContainer, U32 const start, U32 const nbBits)
{
    U32 const regMask = sizeof(bitContainer)*8 - 1;
    /* if start > regMask, bitstream is corrupted, and result is undefined */
    assert(nbBits < BITn_MASK_SIZE);
    return (bitContainer >> (start & regMask)) & BITn_mask[nbBits];
}

MEMn_STATIC FORCE_INLINE_ATTR size_t BITn_getLowerBits(size_t bitContainer, U32 const nbBits)
{
    assert(nbBits < BITn_MASK_SIZE);
    return bitContainer & BITn_mask[nbBits];
}

/*! BITn_lookBits() :
 *  Provides next n bits from local register.
 *  local register is not modified.
 *  On 32-bits, maxNbBits==24.
 *  On 64-bits, maxNbBits==56.
 * @return : value extracted */
MEMn_STATIC  FORCE_INLINE_ATTR size_t BITn_lookBits(const BITn_DStream_t*  bitD, U32 nbBits)
{
    /* arbitrate between double-shift and shift+mask */
#if 1
    /* if bitD->bitsConsumed + nbBits > sizeof(bitD->bitContainer)*8,
     * bitstream is likely corrupted, and result is undefined */
    return BITn_getMiddleBits(bitD->bitContainer, (sizeof(bitD->bitContainer)*8) - bitD->bitsConsumed - nbBits, nbBits);
#else
    /* this code path is slower on my os-x laptop */
    U32 const regMask = sizeof(bitD->bitContainer)*8 - 1;
    return ((bitD->bitContainer << (bitD->bitsConsumed & regMask)) >> 1) >> ((regMask-nbBits) & regMask);
#endif
}

/*! BITn_lookBitsFast() :
 *  unsafe version; only works if nbBits >= 1 */
MEMn_STATIC size_t BITn_lookBitsFast(const BITn_DStream_t* bitD, U32 nbBits)
{
    U32 const regMask = sizeof(bitD->bitContainer)*8 - 1;
    assert(nbBits >= 1);
    return (bitD->bitContainer << (bitD->bitsConsumed & regMask)) >> (((regMask+1)-nbBits) & regMask);
}

MEMn_STATIC FORCE_INLINE_ATTR void BITn_skipBits(BITn_DStream_t* bitD, U32 nbBits)
{
    bitD->bitsConsumed += nbBits;
}

/*! BITn_readBits() :
 *  Read (consume) next n bits from local register and update.
 *  Pay attention to not read more than nbBits contained into local register.
 * @return : extracted value. */
MEMn_STATIC FORCE_INLINE_ATTR size_t BITn_readBits(BITn_DStream_t* bitD, unsigned nbBits)
{
    size_t const value = BITn_lookBits(bitD, nbBits);
    BITn_skipBits(bitD, nbBits);
    return value;
}

/*! BITn_readBitsFast() :
 *  unsafe version; only works only if nbBits >= 1 */
MEMn_STATIC size_t BITn_readBitsFast(BITn_DStream_t* bitD, unsigned nbBits)
{
    size_t const value = BITn_lookBitsFast(bitD, nbBits);
    assert(nbBits >= 1);
    BITn_skipBits(bitD, nbBits);
    return value;
}

/*! BITn_reloadDStreamFast() :
 *  Similar to BITn_reloadDStream(), but with two differences:
 *  1. bitsConsumed <= sizeof(bitD->bitContainer)*8 must hold!
 *  2. Returns BITn_DStream_overflow when bitD->ptr < bitD->limitPtr, at this
 *     point you must use BITn_reloadDStream() to reload.
 */
MEMn_STATIC BITn_DStream_status BITn_reloadDStreamFast(BITn_DStream_t* bitD)
{
    if (UNLIKELY(bitD->ptr < bitD->limitPtr))
        return BITn_DStream_overflow;
    assert(bitD->bitsConsumed <= sizeof(bitD->bitContainer)*8);
    bitD->ptr -= bitD->bitsConsumed >> 3;
    bitD->bitsConsumed &= 7;
    bitD->bitContainer = MEMn_readLEST(bitD->ptr);
    return BITn_DStream_unfinished;
}

/*! BITn_reloadDStream() :
 *  Refill `bitD` from buffer previously set in BITn_initDStream() .
 *  This function is safe, it guarantees it will not read beyond src buffer.
 * @return : status of `BITn_DStream_t` internal register.
 *           when status == BITn_DStream_unfinished, internal register is filled with at least 25 or 57 bits */
MEMn_STATIC BITn_DStream_status BITn_reloadDStream(BITn_DStream_t* bitD)
{
    if (bitD->bitsConsumed > (sizeof(bitD->bitContainer)*8))  /* overflow detected, like end of stream */
        return BITn_DStream_overflow;

    if (bitD->ptr >= bitD->limitPtr) {
        return BITn_reloadDStreamFast(bitD);
    }
    if (bitD->ptr == bitD->start) {
        if (bitD->bitsConsumed < sizeof(bitD->bitContainer)*8) return BITn_DStream_endOfBuffer;
        return BITn_DStream_completed;
    }
    /* start < ptr < limitPtr */
    {   U32 nbBytes = bitD->bitsConsumed >> 3;
        BITn_DStream_status result = BITn_DStream_unfinished;
        if (bitD->ptr - nbBytes < bitD->start) {
            nbBytes = (U32)(bitD->ptr - bitD->start);  /* ptr > start */
            result = BITn_DStream_endOfBuffer;
        }
        bitD->ptr -= nbBytes;
        bitD->bitsConsumed -= nbBytes*8;
        bitD->bitContainer = MEMn_readLEST(bitD->ptr);   /* reminder : srcSize > sizeof(bitD->bitContainer), otherwise bitD->ptr == bitD->start */
        return result;
    }
}

/*! BITn_endOfDStream() :
 * @return : 1 if DStream has _exactly_ reached its end (all bits consumed).
 */
MEMn_STATIC unsigned BITn_endOfDStream(const BITn_DStream_t* DStream)
{
    return ((DStream->ptr == DStream->start) && (DStream->bitsConsumed == sizeof(DStream->bitContainer)*8));
}


#endif /* BITSTREAM_H_MODULE */
