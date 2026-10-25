/* ******************************************************************
 * FSE : Finite State Entropy decoder
 * Copyright (c) Yann Collet, Facebook, Inc.
 *
 *  You can contact the author at :
 *  - FSE source repository : https://github.com/Cyan4973/FiniteStateEntropy
 *  - Public forum : https://groups.google.com/forum/#!forum/lz4c
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
****************************************************************** */


/* **************************************************************
*  Includes
****************************************************************/
#include "debug.h"      /* assert */
#include "bitstream.h"
#include "compiler.h"
#define FSEn_STATIC_LINKING_ONLY
#include "fse.h"
#include "error_private.h"
#define ZSTDn_DEPS_NEED_MALLOC
#include "zstd_deps.h"


/* **************************************************************
*  Error Management
****************************************************************/
#define FSEn_isError ERR_isError
#define FSEn_STATIC_ASSERT(c) DEBUG_STATIC_ASSERT(c)   /* use only *after* variable declarations */


/* **************************************************************
*  Templates
****************************************************************/
/*
  designed to be included
  for type-specific functions (template emulation in C)
  Objective is to write these functions only once, for improved maintenance
*/

/* safety checks */
#ifndef FSEn_FUNCTION_EXTENSION
#  error "FSEn_FUNCTION_EXTENSION must be defined"
#endif
#ifndef FSEn_FUNCTION_TYPE
#  error "FSEn_FUNCTION_TYPE must be defined"
#endif

/* Function names */
#define FSEn_CAT(X,Y) X##Y
#define FSEn_FUNCTION_NAME(X,Y) FSEn_CAT(X,Y)
#define FSEn_TYPE_NAME(X,Y) FSEn_CAT(X,Y)


/* Function templates */
FSEn_DTable* FSEn_createDTable (unsigned tableLog)
{
    if (tableLog > FSEn_TABLELOG_ABSOLUTE_MAX) tableLog = FSEn_TABLELOG_ABSOLUTE_MAX;
    return (FSEn_DTable*)ZSTDn_malloc( FSEn_DTABLE_SIZE_U32(tableLog) * sizeof (U32) );
}

void FSEn_freeDTable (FSEn_DTable* dt)
{
    ZSTDn_free(dt);
}

static size_t FSEn_buildDTable_internal(FSEn_DTable* dt, const short* normalizedCounter, unsigned maxSymbolValue, unsigned tableLog, void* workSpace, size_t wkspSize)
{
    void* const tdPtr = dt+1;   /* because *dt is unsigned, 32-bits aligned on 32-bits */
    FSEn_DECODE_TYPE* const tableDecode = (FSEn_DECODE_TYPE*) (tdPtr);
    U16* symbolNext = (U16*)workSpace;
    BYTE* spread = (BYTE*)(symbolNext + maxSymbolValue + 1);

    U32 const maxSV1 = maxSymbolValue + 1;
    U32 const tableSize = 1 << tableLog;
    U32 highThreshold = tableSize-1;

    /* Sanity Checks */
    if (FSEn_BUILD_DTABLE_WKSP_SIZE(tableLog, maxSymbolValue) > wkspSize) return ERROR(maxSymbolValue_tooLarge);
    if (maxSymbolValue > FSEn_MAX_SYMBOL_VALUE) return ERROR(maxSymbolValue_tooLarge);
    if (tableLog > FSEn_MAX_TABLELOG) return ERROR(tableLog_tooLarge);

    /* Init, lay down lowprob symbols */
    {   FSEn_DTableHeader DTableH;
        DTableH.tableLog = (U16)tableLog;
        DTableH.fastMode = 1;
        {   S16 const largeLimit= (S16)(1 << (tableLog-1));
            U32 s;
            for (s=0; s<maxSV1; s++) {
                if (normalizedCounter[s]==-1) {
                    tableDecode[highThreshold--].symbol = (FSEn_FUNCTION_TYPE)s;
                    symbolNext[s] = 1;
                } else {
                    if (normalizedCounter[s] >= largeLimit) DTableH.fastMode=0;
                    symbolNext[s] = normalizedCounter[s];
        }   }   }
        ZSTDn_memcpy(dt, &DTableH, sizeof(DTableH));
    }

    /* Spread symbols */
    if (highThreshold == tableSize - 1) {
        size_t const tableMask = tableSize-1;
        size_t const step = FSEn_TABLESTEP(tableSize);
        /* First lay down the symbols in order.
         * We use a uint64_t to lay down 8 bytes at a time. This reduces branch
         * misses since small blocks generally have small table logs, so nearly
         * all symbols have counts <= 8. We ensure we have 8 bytes at the end of
         * our buffer to handle the over-write.
         */
        {
            U64 const add = 0x0101010101010101ull;
            size_t pos = 0;
            U64 sv = 0;
            U32 s;
            for (s=0; s<maxSV1; ++s, sv += add) {
                int i;
                int const n = normalizedCounter[s];
                MEMn_write64(spread + pos, sv);
                for (i = 8; i < n; i += 8) {
                    MEMn_write64(spread + pos + i, sv);
                }
                pos += n;
            }
        }
        /* Now we spread those positions across the table.
         * The benefit of doing it in two stages is that we avoid the the
         * variable size inner loop, which caused lots of branch misses.
         * Now we can run through all the positions without any branch misses.
         * We unroll the loop twice, since that is what emperically worked best.
         */
        {
            size_t position = 0;
            size_t s;
            size_t const unroll = 2;
            assert(tableSize % unroll == 0); /* FSEn_MIN_TABLELOG is 5 */
            for (s = 0; s < (size_t)tableSize; s += unroll) {
                size_t u;
                for (u = 0; u < unroll; ++u) {
                    size_t const uPosition = (position + (u * step)) & tableMask;
                    tableDecode[uPosition].symbol = spread[s + u];
                }
                position = (position + (unroll * step)) & tableMask;
            }
            assert(position == 0);
        }
    } else {
        U32 const tableMask = tableSize-1;
        U32 const step = FSEn_TABLESTEP(tableSize);
        U32 s, position = 0;
        for (s=0; s<maxSV1; s++) {
            int i;
            for (i=0; i<normalizedCounter[s]; i++) {
                tableDecode[position].symbol = (FSEn_FUNCTION_TYPE)s;
                position = (position + step) & tableMask;
                while (position > highThreshold) position = (position + step) & tableMask;   /* lowprob area */
        }   }
        if (position!=0) return ERROR(GENERIC);   /* position must reach all cells once, otherwise normalizedCounter is incorrect */
    }

    /* Build Decoding table */
    {   U32 u;
        for (u=0; u<tableSize; u++) {
            FSEn_FUNCTION_TYPE const symbol = (FSEn_FUNCTION_TYPE)(tableDecode[u].symbol);
            U32 const nextState = symbolNext[symbol]++;
            tableDecode[u].nbBits = (BYTE) (tableLog - BITn_highbit32(nextState) );
            tableDecode[u].newState = (U16) ( (nextState << tableDecode[u].nbBits) - tableSize);
    }   }

    return 0;
}

size_t FSEn_buildDTable_wksp(FSEn_DTable* dt, const short* normalizedCounter, unsigned maxSymbolValue, unsigned tableLog, void* workSpace, size_t wkspSize)
{
    return FSEn_buildDTable_internal(dt, normalizedCounter, maxSymbolValue, tableLog, workSpace, wkspSize);
}


#ifndef FSEn_COMMONDEFS_ONLY

/*-*******************************************************
*  Decompression (Byte symbols)
*********************************************************/
size_t FSEn_buildDTable_rle (FSEn_DTable* dt, BYTE symbolValue)
{
    void* ptr = dt;
    FSEn_DTableHeader* const DTableH = (FSEn_DTableHeader*)ptr;
    void* dPtr = dt + 1;
    FSEn_decode_t* const cell = (FSEn_decode_t*)dPtr;

    DTableH->tableLog = 0;
    DTableH->fastMode = 0;

    cell->newState = 0;
    cell->symbol = symbolValue;
    cell->nbBits = 0;

    return 0;
}


size_t FSEn_buildDTable_raw (FSEn_DTable* dt, unsigned nbBits)
{
    void* ptr = dt;
    FSEn_DTableHeader* const DTableH = (FSEn_DTableHeader*)ptr;
    void* dPtr = dt + 1;
    FSEn_decode_t* const dinfo = (FSEn_decode_t*)dPtr;
    const unsigned tableSize = 1 << nbBits;
    const unsigned tableMask = tableSize - 1;
    const unsigned maxSV1 = tableMask+1;
    unsigned s;

    /* Sanity checks */
    if (nbBits < 1) return ERROR(GENERIC);         /* min size */

    /* Build Decoding Table */
    DTableH->tableLog = (U16)nbBits;
    DTableH->fastMode = 1;
    for (s=0; s<maxSV1; s++) {
        dinfo[s].newState = 0;
        dinfo[s].symbol = (BYTE)s;
        dinfo[s].nbBits = (BYTE)nbBits;
    }

    return 0;
}

FORCE_INLINE_TEMPLATE size_t FSEn_decompress_usingDTable_generic(
          void* dst, size_t maxDstSize,
    const void* cSrc, size_t cSrcSize,
    const FSEn_DTable* dt, const unsigned fast)
{
    BYTE* const ostart = (BYTE*) dst;
    BYTE* op = ostart;
    BYTE* const omax = op + maxDstSize;
    BYTE* const olimit = omax-3;

    BITn_DStream_t bitD;
    FSEn_DState_t state1;
    FSEn_DState_t state2;

    /* Init */
    CHECK_F(BITn_initDStream(&bitD, cSrc, cSrcSize));

    FSEn_initDState(&state1, &bitD, dt);
    FSEn_initDState(&state2, &bitD, dt);

#define FSEn_GETSYMBOL(statePtr) fast ? FSEn_decodeSymbolFast(statePtr, &bitD) : FSEn_decodeSymbol(statePtr, &bitD)

    /* 4 symbols per loop */
    for ( ; (BITn_reloadDStream(&bitD)==BITn_DStream_unfinished) & (op<olimit) ; op+=4) {
        op[0] = FSEn_GETSYMBOL(&state1);

        if (FSEn_MAX_TABLELOG*2+7 > sizeof(bitD.bitContainer)*8)    /* This test must be static */
            BITn_reloadDStream(&bitD);

        op[1] = FSEn_GETSYMBOL(&state2);

        if (FSEn_MAX_TABLELOG*4+7 > sizeof(bitD.bitContainer)*8)    /* This test must be static */
            { if (BITn_reloadDStream(&bitD) > BITn_DStream_unfinished) { op+=2; break; } }

        op[2] = FSEn_GETSYMBOL(&state1);

        if (FSEn_MAX_TABLELOG*2+7 > sizeof(bitD.bitContainer)*8)    /* This test must be static */
            BITn_reloadDStream(&bitD);

        op[3] = FSEn_GETSYMBOL(&state2);
    }

    /* tail */
    /* note : BITn_reloadDStream(&bitD) >= FSEn_DStream_partiallyFilled; Ends at exactly BITn_DStream_completed */
    while (1) {
        if (op>(omax-2)) return ERROR(dstSize_tooSmall);
        *op++ = FSEn_GETSYMBOL(&state1);
        if (BITn_reloadDStream(&bitD)==BITn_DStream_overflow) {
            *op++ = FSEn_GETSYMBOL(&state2);
            break;
        }

        if (op>(omax-2)) return ERROR(dstSize_tooSmall);
        *op++ = FSEn_GETSYMBOL(&state2);
        if (BITn_reloadDStream(&bitD)==BITn_DStream_overflow) {
            *op++ = FSEn_GETSYMBOL(&state1);
            break;
    }   }

    return op-ostart;
}


size_t FSEn_decompress_usingDTable(void* dst, size_t originalSize,
                            const void* cSrc, size_t cSrcSize,
                            const FSEn_DTable* dt)
{
    const void* ptr = dt;
    const FSEn_DTableHeader* DTableH = (const FSEn_DTableHeader*)ptr;
    const U32 fastMode = DTableH->fastMode;

    /* select fast mode (static) */
    if (fastMode) return FSEn_decompress_usingDTable_generic(dst, originalSize, cSrc, cSrcSize, dt, 1);
    return FSEn_decompress_usingDTable_generic(dst, originalSize, cSrc, cSrcSize, dt, 0);
}


size_t FSEn_decompress_wksp(void* dst, size_t dstCapacity, const void* cSrc, size_t cSrcSize, unsigned maxLog, void* workSpace, size_t wkspSize)
{
    return FSEn_decompress_wksp_bmi2(dst, dstCapacity, cSrc, cSrcSize, maxLog, workSpace, wkspSize, /* bmi2 */ 0);
}

typedef struct {
    short ncount[FSEn_MAX_SYMBOL_VALUE + 1];
    FSEn_DTable dtable[1]; /* Dynamically sized */
} FSEn_DecompressWksp;


FORCE_INLINE_TEMPLATE size_t FSEn_decompress_wksp_body(
        void* dst, size_t dstCapacity,
        const void* cSrc, size_t cSrcSize,
        unsigned maxLog, void* workSpace, size_t wkspSize,
        int bmi2)
{
    const BYTE* const istart = (const BYTE*)cSrc;
    const BYTE* ip = istart;
    unsigned tableLog;
    unsigned maxSymbolValue = FSEn_MAX_SYMBOL_VALUE;
    FSEn_DecompressWksp* const wksp = (FSEn_DecompressWksp*)workSpace;

    DEBUG_STATIC_ASSERT((FSEn_MAX_SYMBOL_VALUE + 1) % 2 == 0);
    if (wkspSize < sizeof(*wksp)) return ERROR(GENERIC);

    /* normal FSE decoding mode */
    {
        size_t const NCountLength = FSEn_readNCount_bmi2(wksp->ncount, &maxSymbolValue, &tableLog, istart, cSrcSize, bmi2);
        if (FSEn_isError(NCountLength)) return NCountLength;
        if (tableLog > maxLog) return ERROR(tableLog_tooLarge);
        assert(NCountLength <= cSrcSize);
        ip += NCountLength;
        cSrcSize -= NCountLength;
    }

    if (FSEn_DECOMPRESS_WKSP_SIZE(tableLog, maxSymbolValue) > wkspSize) return ERROR(tableLog_tooLarge);
    workSpace = wksp->dtable + FSEn_DTABLE_SIZE_U32(tableLog);
    wkspSize -= sizeof(*wksp) + FSEn_DTABLE_SIZE(tableLog);

    CHECK_F( FSEn_buildDTable_internal(wksp->dtable, wksp->ncount, maxSymbolValue, tableLog, workSpace, wkspSize) );

    {
        const void* ptr = wksp->dtable;
        const FSEn_DTableHeader* DTableH = (const FSEn_DTableHeader*)ptr;
        const U32 fastMode = DTableH->fastMode;

        /* select fast mode (static) */
        if (fastMode) return FSEn_decompress_usingDTable_generic(dst, dstCapacity, ip, cSrcSize, wksp->dtable, 1);
        return FSEn_decompress_usingDTable_generic(dst, dstCapacity, ip, cSrcSize, wksp->dtable, 0);
    }
}

/* Avoids the FORCE_INLINE of the _body() function. */
static size_t FSEn_decompress_wksp_body_default(void* dst, size_t dstCapacity, const void* cSrc, size_t cSrcSize, unsigned maxLog, void* workSpace, size_t wkspSize)
{
    return FSEn_decompress_wksp_body(dst, dstCapacity, cSrc, cSrcSize, maxLog, workSpace, wkspSize, 0);
}

#if DYNAMIC_BMI2
TARGET_ATTRIBUTE("bmi2") static size_t FSEn_decompress_wksp_body_bmi2(void* dst, size_t dstCapacity, const void* cSrc, size_t cSrcSize, unsigned maxLog, void* workSpace, size_t wkspSize)
{
    return FSEn_decompress_wksp_body(dst, dstCapacity, cSrc, cSrcSize, maxLog, workSpace, wkspSize, 1);
}
#endif

size_t FSEn_decompress_wksp_bmi2(void* dst, size_t dstCapacity, const void* cSrc, size_t cSrcSize, unsigned maxLog, void* workSpace, size_t wkspSize, int bmi2)
{
#if DYNAMIC_BMI2
    if (bmi2) {
        return FSEn_decompress_wksp_body_bmi2(dst, dstCapacity, cSrc, cSrcSize, maxLog, workSpace, wkspSize);
    }
#endif
    (void)bmi2;
    return FSEn_decompress_wksp_body_default(dst, dstCapacity, cSrc, cSrcSize, maxLog, workSpace, wkspSize);
}


typedef FSEn_DTable DTable_max_t[FSEn_DTABLE_SIZE_U32(FSEn_MAX_TABLELOG)];



#endif   /* FSEn_COMMONDEFS_ONLY */
