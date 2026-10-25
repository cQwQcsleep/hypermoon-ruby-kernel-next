/* ******************************************************************
 * huff0 huffman decoder,
 * part of Finite State Entropy library
 * Copyright (c) Yann Collet, Facebook, Inc.
 *
 *  You can contact the author at :
 *  - FSE+HUF source repository : https://github.com/Cyan4973/FiniteStateEntropy
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
****************************************************************** */

/* **************************************************************
*  Dependencies
****************************************************************/
#include "../common/zstd_deps.h"  /* ZSTDn_memcpy, ZSTDn_memset */
#include "../common/compiler.h"
#include "../common/bitstream.h"  /* BITn_* */
#include "../common/fse.h"        /* to compress headers */
#define HUFn_STATIC_LINKING_ONLY
#include "../common/huf.h"
#include "../common/error_private.h"

/* **************************************************************
*  Macros
****************************************************************/

/* These two optional macros force the use one way or another of the two
 * Huffman decompression implementations. You can't force in both directions
 * at the same time.
 */
#if defined(HUFn_FORCE_DECOMPRESS_X1) && \
    defined(HUFn_FORCE_DECOMPRESS_X2)
#error "Cannot force the use of the X1 and X2 decoders at the same time!"
#endif


/* **************************************************************
*  Error Management
****************************************************************/
#define HUFn_isError ERR_isError


/* **************************************************************
*  Byte alignment for workSpace management
****************************************************************/
#define HUFn_ALIGN(x, a)         HUFn_ALIGN_MASK((x), (a) - 1)
#define HUFn_ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))


/* **************************************************************
*  BMI2 Variant Wrappers
****************************************************************/
#if DYNAMIC_BMI2

#define HUFn_DGEN(fn)                                                        \
                                                                            \
    static size_t fn##_default(                                             \
                  void* dst,  size_t dstSize,                               \
            const void* cSrc, size_t cSrcSize,                              \
            const HUFn_DTable* DTable)                                       \
    {                                                                       \
        return fn##_body(dst, dstSize, cSrc, cSrcSize, DTable);             \
    }                                                                       \
                                                                            \
    static TARGET_ATTRIBUTE("bmi2") size_t fn##_bmi2(                       \
                  void* dst,  size_t dstSize,                               \
            const void* cSrc, size_t cSrcSize,                              \
            const HUFn_DTable* DTable)                                       \
    {                                                                       \
        return fn##_body(dst, dstSize, cSrc, cSrcSize, DTable);             \
    }                                                                       \
                                                                            \
    static size_t fn(void* dst, size_t dstSize, void const* cSrc,           \
                     size_t cSrcSize, HUFn_DTable const* DTable, int bmi2)   \
    {                                                                       \
        if (bmi2) {                                                         \
            return fn##_bmi2(dst, dstSize, cSrc, cSrcSize, DTable);         \
        }                                                                   \
        return fn##_default(dst, dstSize, cSrc, cSrcSize, DTable);          \
    }

#else

#define HUFn_DGEN(fn)                                                        \
    static size_t fn(void* dst, size_t dstSize, void const* cSrc,           \
                     size_t cSrcSize, HUFn_DTable const* DTable, int bmi2)   \
    {                                                                       \
        (void)bmi2;                                                         \
        return fn##_body(dst, dstSize, cSrc, cSrcSize, DTable);             \
    }

#endif


/*-***************************/
/*  generic DTableDesc       */
/*-***************************/
typedef struct { BYTE maxTableLog; BYTE tableType; BYTE tableLog; BYTE reserved; } DTableDesc;

static DTableDesc HUFn_getDTableDesc(const HUFn_DTable* table)
{
    DTableDesc dtd;
    ZSTDn_memcpy(&dtd, table, sizeof(dtd));
    return dtd;
}


#ifndef HUFn_FORCE_DECOMPRESS_X2

/*-***************************/
/*  single-symbol decoding   */
/*-***************************/
typedef struct { BYTE byte; BYTE nbBits; } HUFn_DEltX1;   /* single-symbol decoding */

/*
 * Packs 4 HUFn_DEltX1 structs into a U64. This is used to lay down 4 entries at
 * a time.
 */
static U64 HUFn_DEltX1_set4(BYTE symbol, BYTE nbBits) {
    U64 D4;
    if (MEMn_isLittleEndian()) {
        D4 = symbol + (nbBits << 8);
    } else {
        D4 = (symbol << 8) + nbBits;
    }
    D4 *= 0x0001000100010001ULL;
    return D4;
}

typedef struct {
        U32 rankVal[HUFn_TABLELOG_ABSOLUTEMAX + 1];
        U32 rankStart[HUFn_TABLELOG_ABSOLUTEMAX + 1];
        U32 statsWksp[HUFn_READ_STATS_WORKSPACE_SIZE_U32];
        BYTE symbols[HUFn_SYMBOLVALUE_MAX + 1];
        BYTE huffWeight[HUFn_SYMBOLVALUE_MAX + 1];
} HUFn_ReadDTableX1_Workspace;


size_t HUFn_readDTableX1_wksp(HUFn_DTable* DTable, const void* src, size_t srcSize, void* workSpace, size_t wkspSize)
{
    return HUFn_readDTableX1_wksp_bmi2(DTable, src, srcSize, workSpace, wkspSize, /* bmi2 */ 0);
}

size_t HUFn_readDTableX1_wksp_bmi2(HUFn_DTable* DTable, const void* src, size_t srcSize, void* workSpace, size_t wkspSize, int bmi2)
{
    U32 tableLog = 0;
    U32 nbSymbols = 0;
    size_t iSize;
    void* const dtPtr = DTable + 1;
    HUFn_DEltX1* const dt = (HUFn_DEltX1*)dtPtr;
    HUFn_ReadDTableX1_Workspace* wksp = (HUFn_ReadDTableX1_Workspace*)workSpace;

    DEBUG_STATIC_ASSERT(HUFn_DECOMPRESS_WORKSPACE_SIZE >= sizeof(*wksp));
    if (sizeof(*wksp) > wkspSize) return ERROR(tableLog_tooLarge);

    DEBUG_STATIC_ASSERT(sizeof(DTableDesc) == sizeof(HUFn_DTable));
    /* ZSTDn_memset(huffWeight, 0, sizeof(huffWeight)); */   /* is not necessary, even though some analyzer complain ... */

    iSize = HUFn_readStats_wksp(wksp->huffWeight, HUFn_SYMBOLVALUE_MAX + 1, wksp->rankVal, &nbSymbols, &tableLog, src, srcSize, wksp->statsWksp, sizeof(wksp->statsWksp), bmi2);
    if (HUFn_isError(iSize)) return iSize;

    /* Table header */
    {   DTableDesc dtd = HUFn_getDTableDesc(DTable);
        if (tableLog > (U32)(dtd.maxTableLog+1)) return ERROR(tableLog_tooLarge);   /* DTable too small, Huffman tree cannot fit in */
        dtd.tableType = 0;
        dtd.tableLog = (BYTE)tableLog;
        ZSTDn_memcpy(DTable, &dtd, sizeof(dtd));
    }

    /* Compute symbols and rankStart given rankVal:
     *
     * rankVal already contains the number of values of each weight.
     *
     * symbols contains the symbols ordered by weight. First are the rankVal[0]
     * weight 0 symbols, followed by the rankVal[1] weight 1 symbols, and so on.
     * symbols[0] is filled (but unused) to avoid a branch.
     *
     * rankStart contains the offset where each rank belongs in the DTable.
     * rankStart[0] is not filled because there are no entries in the table for
     * weight 0.
     */
    {
        int n;
        int nextRankStart = 0;
        int const unroll = 4;
        int const nLimit = (int)nbSymbols - unroll + 1;
        for (n=0; n<(int)tableLog+1; n++) {
            U32 const curr = nextRankStart;
            nextRankStart += wksp->rankVal[n];
            wksp->rankStart[n] = curr;
        }
        for (n=0; n < nLimit; n += unroll) {
            int u;
            for (u=0; u < unroll; ++u) {
                size_t const w = wksp->huffWeight[n+u];
                wksp->symbols[wksp->rankStart[w]++] = (BYTE)(n+u);
            }
        }
        for (; n < (int)nbSymbols; ++n) {
            size_t const w = wksp->huffWeight[n];
            wksp->symbols[wksp->rankStart[w]++] = (BYTE)n;
        }
    }

    /* fill DTable
     * We fill all entries of each weight in order.
     * That way length is a constant for each iteration of the outter loop.
     * We can switch based on the length to a different inner loop which is
     * optimized for that particular case.
     */
    {
        U32 w;
        int symbol=wksp->rankVal[0];
        int rankStart=0;
        for (w=1; w<tableLog+1; ++w) {
            int const symbolCount = wksp->rankVal[w];
            int const length = (1 << w) >> 1;
            int uStart = rankStart;
            BYTE const nbBits = (BYTE)(tableLog + 1 - w);
            int s;
            int u;
            switch (length) {
            case 1:
                for (s=0; s<symbolCount; ++s) {
                    HUFn_DEltX1 D;
                    D.byte = wksp->symbols[symbol + s];
                    D.nbBits = nbBits;
                    dt[uStart] = D;
                    uStart += 1;
                }
                break;
            case 2:
                for (s=0; s<symbolCount; ++s) {
                    HUFn_DEltX1 D;
                    D.byte = wksp->symbols[symbol + s];
                    D.nbBits = nbBits;
                    dt[uStart+0] = D;
                    dt[uStart+1] = D;
                    uStart += 2;
                }
                break;
            case 4:
                for (s=0; s<symbolCount; ++s) {
                    U64 const D4 = HUFn_DEltX1_set4(wksp->symbols[symbol + s], nbBits);
                    MEMn_write64(dt + uStart, D4);
                    uStart += 4;
                }
                break;
            case 8:
                for (s=0; s<symbolCount; ++s) {
                    U64 const D4 = HUFn_DEltX1_set4(wksp->symbols[symbol + s], nbBits);
                    MEMn_write64(dt + uStart, D4);
                    MEMn_write64(dt + uStart + 4, D4);
                    uStart += 8;
                }
                break;
            default:
                for (s=0; s<symbolCount; ++s) {
                    U64 const D4 = HUFn_DEltX1_set4(wksp->symbols[symbol + s], nbBits);
                    for (u=0; u < length; u += 16) {
                        MEMn_write64(dt + uStart + u + 0, D4);
                        MEMn_write64(dt + uStart + u + 4, D4);
                        MEMn_write64(dt + uStart + u + 8, D4);
                        MEMn_write64(dt + uStart + u + 12, D4);
                    }
                    assert(u == length);
                    uStart += length;
                }
                break;
            }
            symbol += symbolCount;
            rankStart += symbolCount * length;
        }
    }
    return iSize;
}

FORCE_INLINE_TEMPLATE BYTE
HUFn_decodeSymbolX1(BITn_DStream_t* Dstream, const HUFn_DEltX1* dt, const U32 dtLog)
{
    size_t const val = BITn_lookBitsFast(Dstream, dtLog); /* note : dtLog >= 1 */
    BYTE const c = dt[val].byte;
    BITn_skipBits(Dstream, dt[val].nbBits);
    return c;
}

#define HUFn_DECODE_SYMBOLX1_0(ptr, DStreamPtr) \
    *ptr++ = HUFn_decodeSymbolX1(DStreamPtr, dt, dtLog)

#define HUFn_DECODE_SYMBOLX1_1(ptr, DStreamPtr)  \
    if (MEMn_64bits() || (HUFn_TABLELOG_MAX<=12)) \
        HUFn_DECODE_SYMBOLX1_0(ptr, DStreamPtr)

#define HUFn_DECODE_SYMBOLX1_2(ptr, DStreamPtr) \
    if (MEMn_64bits()) \
        HUFn_DECODE_SYMBOLX1_0(ptr, DStreamPtr)

HINT_INLINE size_t
HUFn_decodeStreamX1(BYTE* p, BITn_DStream_t* const bitDPtr, BYTE* const pEnd, const HUFn_DEltX1* const dt, const U32 dtLog)
{
    BYTE* const pStart = p;

    /* up to 4 symbols at a time */
    while ((BITn_reloadDStream(bitDPtr) == BITn_DStream_unfinished) & (p < pEnd-3)) {
        HUFn_DECODE_SYMBOLX1_2(p, bitDPtr);
        HUFn_DECODE_SYMBOLX1_1(p, bitDPtr);
        HUFn_DECODE_SYMBOLX1_2(p, bitDPtr);
        HUFn_DECODE_SYMBOLX1_0(p, bitDPtr);
    }

    /* [0-3] symbols remaining */
    if (MEMn_32bits())
        while ((BITn_reloadDStream(bitDPtr) == BITn_DStream_unfinished) & (p < pEnd))
            HUFn_DECODE_SYMBOLX1_0(p, bitDPtr);

    /* no more data to retrieve from bitstream, no need to reload */
    while (p < pEnd)
        HUFn_DECODE_SYMBOLX1_0(p, bitDPtr);

    return pEnd-pStart;
}

FORCE_INLINE_TEMPLATE size_t
HUFn_decompress1X1_usingDTable_internal_body(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFn_DTable* DTable)
{
    BYTE* op = (BYTE*)dst;
    BYTE* const oend = op + dstSize;
    const void* dtPtr = DTable + 1;
    const HUFn_DEltX1* const dt = (const HUFn_DEltX1*)dtPtr;
    BITn_DStream_t bitD;
    DTableDesc const dtd = HUFn_getDTableDesc(DTable);
    U32 const dtLog = dtd.tableLog;

    CHECK_F( BITn_initDStream(&bitD, cSrc, cSrcSize) );

    HUFn_decodeStreamX1(op, &bitD, oend, dt, dtLog);

    if (!BITn_endOfDStream(&bitD)) return ERROR(corruption_detected);

    return dstSize;
}

FORCE_INLINE_TEMPLATE size_t
HUFn_decompress4X1_usingDTable_internal_body(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFn_DTable* DTable)
{
    /* Check */
    if (cSrcSize < 10) return ERROR(corruption_detected);  /* strict minimum : jump table + 1 byte per stream */

    {   const BYTE* const istart = (const BYTE*) cSrc;
        BYTE* const ostart = (BYTE*) dst;
        BYTE* const oend = ostart + dstSize;
        BYTE* const olimit = oend - 3;
        const void* const dtPtr = DTable + 1;
        const HUFn_DEltX1* const dt = (const HUFn_DEltX1*)dtPtr;

        /* Init */
        BITn_DStream_t bitD1;
        BITn_DStream_t bitD2;
        BITn_DStream_t bitD3;
        BITn_DStream_t bitD4;
        size_t const length1 = MEMn_readLE16(istart);
        size_t const length2 = MEMn_readLE16(istart+2);
        size_t const length3 = MEMn_readLE16(istart+4);
        size_t const length4 = cSrcSize - (length1 + length2 + length3 + 6);
        const BYTE* const istart1 = istart + 6;  /* jumpTable */
        const BYTE* const istart2 = istart1 + length1;
        const BYTE* const istart3 = istart2 + length2;
        const BYTE* const istart4 = istart3 + length3;
        const size_t segmentSize = (dstSize+3) / 4;
        BYTE* const opStart2 = ostart + segmentSize;
        BYTE* const opStart3 = opStart2 + segmentSize;
        BYTE* const opStart4 = opStart3 + segmentSize;
        BYTE* op1 = ostart;
        BYTE* op2 = opStart2;
        BYTE* op3 = opStart3;
        BYTE* op4 = opStart4;
        DTableDesc const dtd = HUFn_getDTableDesc(DTable);
        U32 const dtLog = dtd.tableLog;
        U32 endSignal = 1;

        if (length4 > cSrcSize) return ERROR(corruption_detected);   /* overflow */
        CHECK_F( BITn_initDStream(&bitD1, istart1, length1) );
        CHECK_F( BITn_initDStream(&bitD2, istart2, length2) );
        CHECK_F( BITn_initDStream(&bitD3, istart3, length3) );
        CHECK_F( BITn_initDStream(&bitD4, istart4, length4) );

        /* up to 16 symbols per loop (4 symbols per stream) in 64-bit mode */
        for ( ; (endSignal) & (op4 < olimit) ; ) {
            HUFn_DECODE_SYMBOLX1_2(op1, &bitD1);
            HUFn_DECODE_SYMBOLX1_2(op2, &bitD2);
            HUFn_DECODE_SYMBOLX1_2(op3, &bitD3);
            HUFn_DECODE_SYMBOLX1_2(op4, &bitD4);
            HUFn_DECODE_SYMBOLX1_1(op1, &bitD1);
            HUFn_DECODE_SYMBOLX1_1(op2, &bitD2);
            HUFn_DECODE_SYMBOLX1_1(op3, &bitD3);
            HUFn_DECODE_SYMBOLX1_1(op4, &bitD4);
            HUFn_DECODE_SYMBOLX1_2(op1, &bitD1);
            HUFn_DECODE_SYMBOLX1_2(op2, &bitD2);
            HUFn_DECODE_SYMBOLX1_2(op3, &bitD3);
            HUFn_DECODE_SYMBOLX1_2(op4, &bitD4);
            HUFn_DECODE_SYMBOLX1_0(op1, &bitD1);
            HUFn_DECODE_SYMBOLX1_0(op2, &bitD2);
            HUFn_DECODE_SYMBOLX1_0(op3, &bitD3);
            HUFn_DECODE_SYMBOLX1_0(op4, &bitD4);
            endSignal &= BITn_reloadDStreamFast(&bitD1) == BITn_DStream_unfinished;
            endSignal &= BITn_reloadDStreamFast(&bitD2) == BITn_DStream_unfinished;
            endSignal &= BITn_reloadDStreamFast(&bitD3) == BITn_DStream_unfinished;
            endSignal &= BITn_reloadDStreamFast(&bitD4) == BITn_DStream_unfinished;
        }

        /* check corruption */
        /* note : should not be necessary : op# advance in lock step, and we control op4.
         *        but curiously, binary generated by gcc 7.2 & 7.3 with -mbmi2 runs faster when >=1 test is present */
        if (op1 > opStart2) return ERROR(corruption_detected);
        if (op2 > opStart3) return ERROR(corruption_detected);
        if (op3 > opStart4) return ERROR(corruption_detected);
        /* note : op4 supposed already verified within main loop */

        /* finish bitStreams one by one */
        HUFn_decodeStreamX1(op1, &bitD1, opStart2, dt, dtLog);
        HUFn_decodeStreamX1(op2, &bitD2, opStart3, dt, dtLog);
        HUFn_decodeStreamX1(op3, &bitD3, opStart4, dt, dtLog);
        HUFn_decodeStreamX1(op4, &bitD4, oend,     dt, dtLog);

        /* check */
        { U32 const endCheck = BITn_endOfDStream(&bitD1) & BITn_endOfDStream(&bitD2) & BITn_endOfDStream(&bitD3) & BITn_endOfDStream(&bitD4);
          if (!endCheck) return ERROR(corruption_detected); }

        /* decoded size */
        return dstSize;
    }
}


typedef size_t (*HUFn_decompress_usingDTable_t)(void *dst, size_t dstSize,
                                               const void *cSrc,
                                               size_t cSrcSize,
                                               const HUFn_DTable *DTable);

HUFn_DGEN(HUFn_decompress1X1_usingDTable_internal)
HUFn_DGEN(HUFn_decompress4X1_usingDTable_internal)



size_t HUFn_decompress1X1_usingDTable(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFn_DTable* DTable)
{
    DTableDesc dtd = HUFn_getDTableDesc(DTable);
    if (dtd.tableType != 0) return ERROR(GENERIC);
    return HUFn_decompress1X1_usingDTable_internal(dst, dstSize, cSrc, cSrcSize, DTable, /* bmi2 */ 0);
}

size_t HUFn_decompress1X1_DCtx_wksp(HUFn_DTable* DCtx, void* dst, size_t dstSize,
                                   const void* cSrc, size_t cSrcSize,
                                   void* workSpace, size_t wkspSize)
{
    const BYTE* ip = (const BYTE*) cSrc;

    size_t const hSize = HUFn_readDTableX1_wksp(DCtx, cSrc, cSrcSize, workSpace, wkspSize);
    if (HUFn_isError(hSize)) return hSize;
    if (hSize >= cSrcSize) return ERROR(srcSize_wrong);
    ip += hSize; cSrcSize -= hSize;

    return HUFn_decompress1X1_usingDTable_internal(dst, dstSize, ip, cSrcSize, DCtx, /* bmi2 */ 0);
}


size_t HUFn_decompress4X1_usingDTable(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFn_DTable* DTable)
{
    DTableDesc dtd = HUFn_getDTableDesc(DTable);
    if (dtd.tableType != 0) return ERROR(GENERIC);
    return HUFn_decompress4X1_usingDTable_internal(dst, dstSize, cSrc, cSrcSize, DTable, /* bmi2 */ 0);
}

static size_t HUFn_decompress4X1_DCtx_wksp_bmi2(HUFn_DTable* dctx, void* dst, size_t dstSize,
                                   const void* cSrc, size_t cSrcSize,
                                   void* workSpace, size_t wkspSize, int bmi2)
{
    const BYTE* ip = (const BYTE*) cSrc;

    size_t const hSize = HUFn_readDTableX1_wksp_bmi2(dctx, cSrc, cSrcSize, workSpace, wkspSize, bmi2);
    if (HUFn_isError(hSize)) return hSize;
    if (hSize >= cSrcSize) return ERROR(srcSize_wrong);
    ip += hSize; cSrcSize -= hSize;

    return HUFn_decompress4X1_usingDTable_internal(dst, dstSize, ip, cSrcSize, dctx, bmi2);
}

size_t HUFn_decompress4X1_DCtx_wksp(HUFn_DTable* dctx, void* dst, size_t dstSize,
                                   const void* cSrc, size_t cSrcSize,
                                   void* workSpace, size_t wkspSize)
{
    return HUFn_decompress4X1_DCtx_wksp_bmi2(dctx, dst, dstSize, cSrc, cSrcSize, workSpace, wkspSize, 0);
}


#endif /* HUFn_FORCE_DECOMPRESS_X2 */


#ifndef HUFn_FORCE_DECOMPRESS_X1

/* *************************/
/* double-symbols decoding */
/* *************************/

typedef struct { U16 sequence; BYTE nbBits; BYTE length; } HUFn_DEltX2;  /* double-symbols decoding */
typedef struct { BYTE symbol; BYTE weight; } sortedSymbol_t;
typedef U32 rankValCol_t[HUFn_TABLELOG_MAX + 1];
typedef rankValCol_t rankVal_t[HUFn_TABLELOG_MAX];


/* HUFn_fillDTableX2Level2() :
 * `rankValOrigin` must be a table of at least (HUFn_TABLELOG_MAX + 1) U32 */
static void HUFn_fillDTableX2Level2(HUFn_DEltX2* DTable, U32 sizeLog, const U32 consumed,
                           const U32* rankValOrigin, const int minWeight,
                           const sortedSymbol_t* sortedSymbols, const U32 sortedListSize,
                           U32 nbBitsBaseline, U16 baseSeq, U32* wksp, size_t wkspSize)
{
    HUFn_DEltX2 DElt;
    U32* rankVal = wksp;

    assert(wkspSize >= HUFn_TABLELOG_MAX + 1);
    (void)wkspSize;
    /* get pre-calculated rankVal */
    ZSTDn_memcpy(rankVal, rankValOrigin, sizeof(U32) * (HUFn_TABLELOG_MAX + 1));

    /* fill skipped values */
    if (minWeight>1) {
        U32 i, skipSize = rankVal[minWeight];
        MEMn_writeLE16(&(DElt.sequence), baseSeq);
        DElt.nbBits   = (BYTE)(consumed);
        DElt.length   = 1;
        for (i = 0; i < skipSize; i++)
            DTable[i] = DElt;
    }

    /* fill DTable */
    {   U32 s; for (s=0; s<sortedListSize; s++) {   /* note : sortedSymbols already skipped */
            const U32 symbol = sortedSymbols[s].symbol;
            const U32 weight = sortedSymbols[s].weight;
            const U32 nbBits = nbBitsBaseline - weight;
            const U32 length = 1 << (sizeLog-nbBits);
            const U32 start = rankVal[weight];
            U32 i = start;
            const U32 end = start + length;

            MEMn_writeLE16(&(DElt.sequence), (U16)(baseSeq + (symbol << 8)));
            DElt.nbBits = (BYTE)(nbBits + consumed);
            DElt.length = 2;
            do { DTable[i++] = DElt; } while (i<end);   /* since length >= 1 */

            rankVal[weight] += length;
    }   }
}


static void HUFn_fillDTableX2(HUFn_DEltX2* DTable, const U32 targetLog,
                           const sortedSymbol_t* sortedList, const U32 sortedListSize,
                           const U32* rankStart, rankVal_t rankValOrigin, const U32 maxWeight,
                           const U32 nbBitsBaseline, U32* wksp, size_t wkspSize)
{
    U32* rankVal = wksp;
    const int scaleLog = nbBitsBaseline - targetLog;   /* note : targetLog >= srcLog, hence scaleLog <= 1 */
    const U32 minBits  = nbBitsBaseline - maxWeight;
    U32 s;

    assert(wkspSize >= HUFn_TABLELOG_MAX + 1);
    wksp += HUFn_TABLELOG_MAX + 1;
    wkspSize -= HUFn_TABLELOG_MAX + 1;

    ZSTDn_memcpy(rankVal, rankValOrigin, sizeof(U32) * (HUFn_TABLELOG_MAX + 1));

    /* fill DTable */
    for (s=0; s<sortedListSize; s++) {
        const U16 symbol = sortedList[s].symbol;
        const U32 weight = sortedList[s].weight;
        const U32 nbBits = nbBitsBaseline - weight;
        const U32 start = rankVal[weight];
        const U32 length = 1 << (targetLog-nbBits);

        if (targetLog-nbBits >= minBits) {   /* enough room for a second symbol */
            U32 sortedRank;
            int minWeight = nbBits + scaleLog;
            if (minWeight < 1) minWeight = 1;
            sortedRank = rankStart[minWeight];
            HUFn_fillDTableX2Level2(DTable+start, targetLog-nbBits, nbBits,
                           rankValOrigin[nbBits], minWeight,
                           sortedList+sortedRank, sortedListSize-sortedRank,
                           nbBitsBaseline, symbol, wksp, wkspSize);
        } else {
            HUFn_DEltX2 DElt;
            MEMn_writeLE16(&(DElt.sequence), symbol);
            DElt.nbBits = (BYTE)(nbBits);
            DElt.length = 1;
            {   U32 const end = start + length;
                U32 u;
                for (u = start; u < end; u++) DTable[u] = DElt;
        }   }
        rankVal[weight] += length;
    }
}

typedef struct {
    rankValCol_t rankVal[HUFn_TABLELOG_MAX];
    U32 rankStats[HUFn_TABLELOG_MAX + 1];
    U32 rankStart0[HUFn_TABLELOG_MAX + 2];
    sortedSymbol_t sortedSymbol[HUFn_SYMBOLVALUE_MAX + 1];
    BYTE weightList[HUFn_SYMBOLVALUE_MAX + 1];
    U32 calleeWksp[HUFn_READ_STATS_WORKSPACE_SIZE_U32];
} HUFn_ReadDTableX2_Workspace;

size_t HUFn_readDTableX2_wksp(HUFn_DTable* DTable,
                       const void* src, size_t srcSize,
                             void* workSpace, size_t wkspSize)
{
    U32 tableLog, maxW, sizeOfSort, nbSymbols;
    DTableDesc dtd = HUFn_getDTableDesc(DTable);
    U32 const maxTableLog = dtd.maxTableLog;
    size_t iSize;
    void* dtPtr = DTable+1;   /* force compiler to avoid strict-aliasing */
    HUFn_DEltX2* const dt = (HUFn_DEltX2*)dtPtr;
    U32 *rankStart;

    HUFn_ReadDTableX2_Workspace* const wksp = (HUFn_ReadDTableX2_Workspace*)workSpace;

    if (sizeof(*wksp) > wkspSize) return ERROR(GENERIC);

    rankStart = wksp->rankStart0 + 1;
    ZSTDn_memset(wksp->rankStats, 0, sizeof(wksp->rankStats));
    ZSTDn_memset(wksp->rankStart0, 0, sizeof(wksp->rankStart0));

    DEBUG_STATIC_ASSERT(sizeof(HUFn_DEltX2) == sizeof(HUFn_DTable));   /* if compiler fails here, assertion is wrong */
    if (maxTableLog > HUFn_TABLELOG_MAX) return ERROR(tableLog_tooLarge);
    /* ZSTDn_memset(weightList, 0, sizeof(weightList)); */  /* is not necessary, even though some analyzer complain ... */

    iSize = HUFn_readStats_wksp(wksp->weightList, HUFn_SYMBOLVALUE_MAX + 1, wksp->rankStats, &nbSymbols, &tableLog, src, srcSize, wksp->calleeWksp, sizeof(wksp->calleeWksp), /* bmi2 */ 0);
    if (HUFn_isError(iSize)) return iSize;

    /* check result */
    if (tableLog > maxTableLog) return ERROR(tableLog_tooLarge);   /* DTable can't fit code depth */

    /* find maxWeight */
    for (maxW = tableLog; wksp->rankStats[maxW]==0; maxW--) {}  /* necessarily finds a solution before 0 */

    /* Get start index of each weight */
    {   U32 w, nextRankStart = 0;
        for (w=1; w<maxW+1; w++) {
            U32 curr = nextRankStart;
            nextRankStart += wksp->rankStats[w];
            rankStart[w] = curr;
        }
        rankStart[0] = nextRankStart;   /* put all 0w symbols at the end of sorted list*/
        sizeOfSort = nextRankStart;
    }

    /* sort symbols by weight */
    {   U32 s;
        for (s=0; s<nbSymbols; s++) {
            U32 const w = wksp->weightList[s];
            U32 const r = rankStart[w]++;
            wksp->sortedSymbol[r].symbol = (BYTE)s;
            wksp->sortedSymbol[r].weight = (BYTE)w;
        }
        rankStart[0] = 0;   /* forget 0w symbols; this is beginning of weight(1) */
    }

    /* Build rankVal */
    {   U32* const rankVal0 = wksp->rankVal[0];
        {   int const rescale = (maxTableLog-tableLog) - 1;   /* tableLog <= maxTableLog */
            U32 nextRankVal = 0;
            U32 w;
            for (w=1; w<maxW+1; w++) {
                U32 curr = nextRankVal;
                nextRankVal += wksp->rankStats[w] << (w+rescale);
                rankVal0[w] = curr;
        }   }
        {   U32 const minBits = tableLog+1 - maxW;
            U32 consumed;
            for (consumed = minBits; consumed < maxTableLog - minBits + 1; consumed++) {
                U32* const rankValPtr = wksp->rankVal[consumed];
                U32 w;
                for (w = 1; w < maxW+1; w++) {
                    rankValPtr[w] = rankVal0[w] >> consumed;
    }   }   }   }

    HUFn_fillDTableX2(dt, maxTableLog,
                   wksp->sortedSymbol, sizeOfSort,
                   wksp->rankStart0, wksp->rankVal, maxW,
                   tableLog+1,
                   wksp->calleeWksp, sizeof(wksp->calleeWksp) / sizeof(U32));

    dtd.tableLog = (BYTE)maxTableLog;
    dtd.tableType = 1;
    ZSTDn_memcpy(DTable, &dtd, sizeof(dtd));
    return iSize;
}


FORCE_INLINE_TEMPLATE U32
HUFn_decodeSymbolX2(void* op, BITn_DStream_t* DStream, const HUFn_DEltX2* dt, const U32 dtLog)
{
    size_t const val = BITn_lookBitsFast(DStream, dtLog);   /* note : dtLog >= 1 */
    ZSTDn_memcpy(op, dt+val, 2);
    BITn_skipBits(DStream, dt[val].nbBits);
    return dt[val].length;
}

FORCE_INLINE_TEMPLATE U32
HUFn_decodeLastSymbolX2(void* op, BITn_DStream_t* DStream, const HUFn_DEltX2* dt, const U32 dtLog)
{
    size_t const val = BITn_lookBitsFast(DStream, dtLog);   /* note : dtLog >= 1 */
    ZSTDn_memcpy(op, dt+val, 1);
    if (dt[val].length==1) BITn_skipBits(DStream, dt[val].nbBits);
    else {
        if (DStream->bitsConsumed < (sizeof(DStream->bitContainer)*8)) {
            BITn_skipBits(DStream, dt[val].nbBits);
            if (DStream->bitsConsumed > (sizeof(DStream->bitContainer)*8))
                /* ugly hack; works only because it's the last symbol. Note : can't easily extract nbBits from just this symbol */
                DStream->bitsConsumed = (sizeof(DStream->bitContainer)*8);
    }   }
    return 1;
}

#define HUFn_DECODE_SYMBOLX2_0(ptr, DStreamPtr) \
    ptr += HUFn_decodeSymbolX2(ptr, DStreamPtr, dt, dtLog)

#define HUFn_DECODE_SYMBOLX2_1(ptr, DStreamPtr) \
    if (MEMn_64bits() || (HUFn_TABLELOG_MAX<=12)) \
        ptr += HUFn_decodeSymbolX2(ptr, DStreamPtr, dt, dtLog)

#define HUFn_DECODE_SYMBOLX2_2(ptr, DStreamPtr) \
    if (MEMn_64bits()) \
        ptr += HUFn_decodeSymbolX2(ptr, DStreamPtr, dt, dtLog)

HINT_INLINE size_t
HUFn_decodeStreamX2(BYTE* p, BITn_DStream_t* bitDPtr, BYTE* const pEnd,
                const HUFn_DEltX2* const dt, const U32 dtLog)
{
    BYTE* const pStart = p;

    /* up to 8 symbols at a time */
    while ((BITn_reloadDStream(bitDPtr) == BITn_DStream_unfinished) & (p < pEnd-(sizeof(bitDPtr->bitContainer)-1))) {
        HUFn_DECODE_SYMBOLX2_2(p, bitDPtr);
        HUFn_DECODE_SYMBOLX2_1(p, bitDPtr);
        HUFn_DECODE_SYMBOLX2_2(p, bitDPtr);
        HUFn_DECODE_SYMBOLX2_0(p, bitDPtr);
    }

    /* closer to end : up to 2 symbols at a time */
    while ((BITn_reloadDStream(bitDPtr) == BITn_DStream_unfinished) & (p <= pEnd-2))
        HUFn_DECODE_SYMBOLX2_0(p, bitDPtr);

    while (p <= pEnd-2)
        HUFn_DECODE_SYMBOLX2_0(p, bitDPtr);   /* no need to reload : reached the end of DStream */

    if (p < pEnd)
        p += HUFn_decodeLastSymbolX2(p, bitDPtr, dt, dtLog);

    return p-pStart;
}

FORCE_INLINE_TEMPLATE size_t
HUFn_decompress1X2_usingDTable_internal_body(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFn_DTable* DTable)
{
    BITn_DStream_t bitD;

    /* Init */
    CHECK_F( BITn_initDStream(&bitD, cSrc, cSrcSize) );

    /* decode */
    {   BYTE* const ostart = (BYTE*) dst;
        BYTE* const oend = ostart + dstSize;
        const void* const dtPtr = DTable+1;   /* force compiler to not use strict-aliasing */
        const HUFn_DEltX2* const dt = (const HUFn_DEltX2*)dtPtr;
        DTableDesc const dtd = HUFn_getDTableDesc(DTable);
        HUFn_decodeStreamX2(ostart, &bitD, oend, dt, dtd.tableLog);
    }

    /* check */
    if (!BITn_endOfDStream(&bitD)) return ERROR(corruption_detected);

    /* decoded size */
    return dstSize;
}

FORCE_INLINE_TEMPLATE size_t
HUFn_decompress4X2_usingDTable_internal_body(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFn_DTable* DTable)
{
    if (cSrcSize < 10) return ERROR(corruption_detected);   /* strict minimum : jump table + 1 byte per stream */

    {   const BYTE* const istart = (const BYTE*) cSrc;
        BYTE* const ostart = (BYTE*) dst;
        BYTE* const oend = ostart + dstSize;
        BYTE* const olimit = oend - (sizeof(size_t)-1);
        const void* const dtPtr = DTable+1;
        const HUFn_DEltX2* const dt = (const HUFn_DEltX2*)dtPtr;

        /* Init */
        BITn_DStream_t bitD1;
        BITn_DStream_t bitD2;
        BITn_DStream_t bitD3;
        BITn_DStream_t bitD4;
        size_t const length1 = MEMn_readLE16(istart);
        size_t const length2 = MEMn_readLE16(istart+2);
        size_t const length3 = MEMn_readLE16(istart+4);
        size_t const length4 = cSrcSize - (length1 + length2 + length3 + 6);
        const BYTE* const istart1 = istart + 6;  /* jumpTable */
        const BYTE* const istart2 = istart1 + length1;
        const BYTE* const istart3 = istart2 + length2;
        const BYTE* const istart4 = istart3 + length3;
        size_t const segmentSize = (dstSize+3) / 4;
        BYTE* const opStart2 = ostart + segmentSize;
        BYTE* const opStart3 = opStart2 + segmentSize;
        BYTE* const opStart4 = opStart3 + segmentSize;
        BYTE* op1 = ostart;
        BYTE* op2 = opStart2;
        BYTE* op3 = opStart3;
        BYTE* op4 = opStart4;
        U32 endSignal = 1;
        DTableDesc const dtd = HUFn_getDTableDesc(DTable);
        U32 const dtLog = dtd.tableLog;

        if (length4 > cSrcSize) return ERROR(corruption_detected);   /* overflow */
        CHECK_F( BITn_initDStream(&bitD1, istart1, length1) );
        CHECK_F( BITn_initDStream(&bitD2, istart2, length2) );
        CHECK_F( BITn_initDStream(&bitD3, istart3, length3) );
        CHECK_F( BITn_initDStream(&bitD4, istart4, length4) );

        /* 16-32 symbols per loop (4-8 symbols per stream) */
        for ( ; (endSignal) & (op4 < olimit); ) {
#if defined(__clang__) && (defined(__x86_64__) || defined(__i386__))
            HUFn_DECODE_SYMBOLX2_2(op1, &bitD1);
            HUFn_DECODE_SYMBOLX2_1(op1, &bitD1);
            HUFn_DECODE_SYMBOLX2_2(op1, &bitD1);
            HUFn_DECODE_SYMBOLX2_0(op1, &bitD1);
            HUFn_DECODE_SYMBOLX2_2(op2, &bitD2);
            HUFn_DECODE_SYMBOLX2_1(op2, &bitD2);
            HUFn_DECODE_SYMBOLX2_2(op2, &bitD2);
            HUFn_DECODE_SYMBOLX2_0(op2, &bitD2);
            endSignal &= BITn_reloadDStreamFast(&bitD1) == BITn_DStream_unfinished;
            endSignal &= BITn_reloadDStreamFast(&bitD2) == BITn_DStream_unfinished;
            HUFn_DECODE_SYMBOLX2_2(op3, &bitD3);
            HUFn_DECODE_SYMBOLX2_1(op3, &bitD3);
            HUFn_DECODE_SYMBOLX2_2(op3, &bitD3);
            HUFn_DECODE_SYMBOLX2_0(op3, &bitD3);
            HUFn_DECODE_SYMBOLX2_2(op4, &bitD4);
            HUFn_DECODE_SYMBOLX2_1(op4, &bitD4);
            HUFn_DECODE_SYMBOLX2_2(op4, &bitD4);
            HUFn_DECODE_SYMBOLX2_0(op4, &bitD4);
            endSignal &= BITn_reloadDStreamFast(&bitD3) == BITn_DStream_unfinished;
            endSignal &= BITn_reloadDStreamFast(&bitD4) == BITn_DStream_unfinished;
#else
            HUFn_DECODE_SYMBOLX2_2(op1, &bitD1);
            HUFn_DECODE_SYMBOLX2_2(op2, &bitD2);
            HUFn_DECODE_SYMBOLX2_2(op3, &bitD3);
            HUFn_DECODE_SYMBOLX2_2(op4, &bitD4);
            HUFn_DECODE_SYMBOLX2_1(op1, &bitD1);
            HUFn_DECODE_SYMBOLX2_1(op2, &bitD2);
            HUFn_DECODE_SYMBOLX2_1(op3, &bitD3);
            HUFn_DECODE_SYMBOLX2_1(op4, &bitD4);
            HUFn_DECODE_SYMBOLX2_2(op1, &bitD1);
            HUFn_DECODE_SYMBOLX2_2(op2, &bitD2);
            HUFn_DECODE_SYMBOLX2_2(op3, &bitD3);
            HUFn_DECODE_SYMBOLX2_2(op4, &bitD4);
            HUFn_DECODE_SYMBOLX2_0(op1, &bitD1);
            HUFn_DECODE_SYMBOLX2_0(op2, &bitD2);
            HUFn_DECODE_SYMBOLX2_0(op3, &bitD3);
            HUFn_DECODE_SYMBOLX2_0(op4, &bitD4);
            endSignal = (U32)LIKELY((U32)
                        (BITn_reloadDStreamFast(&bitD1) == BITn_DStream_unfinished)
                      & (BITn_reloadDStreamFast(&bitD2) == BITn_DStream_unfinished)
                      & (BITn_reloadDStreamFast(&bitD3) == BITn_DStream_unfinished)
                      & (BITn_reloadDStreamFast(&bitD4) == BITn_DStream_unfinished));
#endif
        }

        /* check corruption */
        if (op1 > opStart2) return ERROR(corruption_detected);
        if (op2 > opStart3) return ERROR(corruption_detected);
        if (op3 > opStart4) return ERROR(corruption_detected);
        /* note : op4 already verified within main loop */

        /* finish bitStreams one by one */
        HUFn_decodeStreamX2(op1, &bitD1, opStart2, dt, dtLog);
        HUFn_decodeStreamX2(op2, &bitD2, opStart3, dt, dtLog);
        HUFn_decodeStreamX2(op3, &bitD3, opStart4, dt, dtLog);
        HUFn_decodeStreamX2(op4, &bitD4, oend,     dt, dtLog);

        /* check */
        { U32 const endCheck = BITn_endOfDStream(&bitD1) & BITn_endOfDStream(&bitD2) & BITn_endOfDStream(&bitD3) & BITn_endOfDStream(&bitD4);
          if (!endCheck) return ERROR(corruption_detected); }

        /* decoded size */
        return dstSize;
    }
}

HUFn_DGEN(HUFn_decompress1X2_usingDTable_internal)
HUFn_DGEN(HUFn_decompress4X2_usingDTable_internal)

size_t HUFn_decompress1X2_usingDTable(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFn_DTable* DTable)
{
    DTableDesc dtd = HUFn_getDTableDesc(DTable);
    if (dtd.tableType != 1) return ERROR(GENERIC);
    return HUFn_decompress1X2_usingDTable_internal(dst, dstSize, cSrc, cSrcSize, DTable, /* bmi2 */ 0);
}

size_t HUFn_decompress1X2_DCtx_wksp(HUFn_DTable* DCtx, void* dst, size_t dstSize,
                                   const void* cSrc, size_t cSrcSize,
                                   void* workSpace, size_t wkspSize)
{
    const BYTE* ip = (const BYTE*) cSrc;

    size_t const hSize = HUFn_readDTableX2_wksp(DCtx, cSrc, cSrcSize,
                                               workSpace, wkspSize);
    if (HUFn_isError(hSize)) return hSize;
    if (hSize >= cSrcSize) return ERROR(srcSize_wrong);
    ip += hSize; cSrcSize -= hSize;

    return HUFn_decompress1X2_usingDTable_internal(dst, dstSize, ip, cSrcSize, DCtx, /* bmi2 */ 0);
}


size_t HUFn_decompress4X2_usingDTable(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const HUFn_DTable* DTable)
{
    DTableDesc dtd = HUFn_getDTableDesc(DTable);
    if (dtd.tableType != 1) return ERROR(GENERIC);
    return HUFn_decompress4X2_usingDTable_internal(dst, dstSize, cSrc, cSrcSize, DTable, /* bmi2 */ 0);
}

static size_t HUFn_decompress4X2_DCtx_wksp_bmi2(HUFn_DTable* dctx, void* dst, size_t dstSize,
                                   const void* cSrc, size_t cSrcSize,
                                   void* workSpace, size_t wkspSize, int bmi2)
{
    const BYTE* ip = (const BYTE*) cSrc;

    size_t hSize = HUFn_readDTableX2_wksp(dctx, cSrc, cSrcSize,
                                         workSpace, wkspSize);
    if (HUFn_isError(hSize)) return hSize;
    if (hSize >= cSrcSize) return ERROR(srcSize_wrong);
    ip += hSize; cSrcSize -= hSize;

    return HUFn_decompress4X2_usingDTable_internal(dst, dstSize, ip, cSrcSize, dctx, bmi2);
}

size_t HUFn_decompress4X2_DCtx_wksp(HUFn_DTable* dctx, void* dst, size_t dstSize,
                                   const void* cSrc, size_t cSrcSize,
                                   void* workSpace, size_t wkspSize)
{
    return HUFn_decompress4X2_DCtx_wksp_bmi2(dctx, dst, dstSize, cSrc, cSrcSize, workSpace, wkspSize, /* bmi2 */ 0);
}


#endif /* HUFn_FORCE_DECOMPRESS_X1 */


/* ***********************************/
/* Universal decompression selectors */
/* ***********************************/

size_t HUFn_decompress1X_usingDTable(void* dst, size_t maxDstSize,
                                    const void* cSrc, size_t cSrcSize,
                                    const HUFn_DTable* DTable)
{
    DTableDesc const dtd = HUFn_getDTableDesc(DTable);
#if defined(HUFn_FORCE_DECOMPRESS_X1)
    (void)dtd;
    assert(dtd.tableType == 0);
    return HUFn_decompress1X1_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, /* bmi2 */ 0);
#elif defined(HUFn_FORCE_DECOMPRESS_X2)
    (void)dtd;
    assert(dtd.tableType == 1);
    return HUFn_decompress1X2_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, /* bmi2 */ 0);
#else
    return dtd.tableType ? HUFn_decompress1X2_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, /* bmi2 */ 0) :
                           HUFn_decompress1X1_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, /* bmi2 */ 0);
#endif
}

size_t HUFn_decompress4X_usingDTable(void* dst, size_t maxDstSize,
                                    const void* cSrc, size_t cSrcSize,
                                    const HUFn_DTable* DTable)
{
    DTableDesc const dtd = HUFn_getDTableDesc(DTable);
#if defined(HUFn_FORCE_DECOMPRESS_X1)
    (void)dtd;
    assert(dtd.tableType == 0);
    return HUFn_decompress4X1_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, /* bmi2 */ 0);
#elif defined(HUFn_FORCE_DECOMPRESS_X2)
    (void)dtd;
    assert(dtd.tableType == 1);
    return HUFn_decompress4X2_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, /* bmi2 */ 0);
#else
    return dtd.tableType ? HUFn_decompress4X2_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, /* bmi2 */ 0) :
                           HUFn_decompress4X1_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, /* bmi2 */ 0);
#endif
}


#if !defined(HUFn_FORCE_DECOMPRESS_X1) && !defined(HUFn_FORCE_DECOMPRESS_X2)
typedef struct { U32 tableTime; U32 decode256Time; } algo_time_t;
static const algo_time_t algoTime[16 /* Quantization */][3 /* single, double, quad */] =
{
    /* single, double, quad */
    {{0,0}, {1,1}, {2,2}},  /* Q==0 : impossible */
    {{0,0}, {1,1}, {2,2}},  /* Q==1 : impossible */
    {{  38,130}, {1313, 74}, {2151, 38}},   /* Q == 2 : 12-18% */
    {{ 448,128}, {1353, 74}, {2238, 41}},   /* Q == 3 : 18-25% */
    {{ 556,128}, {1353, 74}, {2238, 47}},   /* Q == 4 : 25-32% */
    {{ 714,128}, {1418, 74}, {2436, 53}},   /* Q == 5 : 32-38% */
    {{ 883,128}, {1437, 74}, {2464, 61}},   /* Q == 6 : 38-44% */
    {{ 897,128}, {1515, 75}, {2622, 68}},   /* Q == 7 : 44-50% */
    {{ 926,128}, {1613, 75}, {2730, 75}},   /* Q == 8 : 50-56% */
    {{ 947,128}, {1729, 77}, {3359, 77}},   /* Q == 9 : 56-62% */
    {{1107,128}, {2083, 81}, {4006, 84}},   /* Q ==10 : 62-69% */
    {{1177,128}, {2379, 87}, {4785, 88}},   /* Q ==11 : 69-75% */
    {{1242,128}, {2415, 93}, {5155, 84}},   /* Q ==12 : 75-81% */
    {{1349,128}, {2644,106}, {5260,106}},   /* Q ==13 : 81-87% */
    {{1455,128}, {2422,124}, {4174,124}},   /* Q ==14 : 87-93% */
    {{ 722,128}, {1891,145}, {1936,146}},   /* Q ==15 : 93-99% */
};
#endif

/* HUFn_selectDecoder() :
 *  Tells which decoder is likely to decode faster,
 *  based on a set of pre-computed metrics.
 * @return : 0==HUFn_decompress4X1, 1==HUFn_decompress4X2 .
 *  Assumption : 0 < dstSize <= 128 KB */
U32 HUFn_selectDecoder (size_t dstSize, size_t cSrcSize)
{
    assert(dstSize > 0);
    assert(dstSize <= 128*1024);
#if defined(HUFn_FORCE_DECOMPRESS_X1)
    (void)dstSize;
    (void)cSrcSize;
    return 0;
#elif defined(HUFn_FORCE_DECOMPRESS_X2)
    (void)dstSize;
    (void)cSrcSize;
    return 1;
#else
    /* decoder timing evaluation */
    {   U32 const Q = (cSrcSize >= dstSize) ? 15 : (U32)(cSrcSize * 16 / dstSize);   /* Q < 16 */
        U32 const D256 = (U32)(dstSize >> 8);
        U32 const DTime0 = algoTime[Q][0].tableTime + (algoTime[Q][0].decode256Time * D256);
        U32 DTime1 = algoTime[Q][1].tableTime + (algoTime[Q][1].decode256Time * D256);
        DTime1 += DTime1 >> 3;  /* advantage to algorithm using less memory, to reduce cache eviction */
        return DTime1 < DTime0;
    }
#endif
}


size_t HUFn_decompress4X_hufOnly_wksp(HUFn_DTable* dctx, void* dst,
                                     size_t dstSize, const void* cSrc,
                                     size_t cSrcSize, void* workSpace,
                                     size_t wkspSize)
{
    /* validation checks */
    if (dstSize == 0) return ERROR(dstSize_tooSmall);
    if (cSrcSize == 0) return ERROR(corruption_detected);

    {   U32 const algoNb = HUFn_selectDecoder(dstSize, cSrcSize);
#if defined(HUFn_FORCE_DECOMPRESS_X1)
        (void)algoNb;
        assert(algoNb == 0);
        return HUFn_decompress4X1_DCtx_wksp(dctx, dst, dstSize, cSrc, cSrcSize, workSpace, wkspSize);
#elif defined(HUFn_FORCE_DECOMPRESS_X2)
        (void)algoNb;
        assert(algoNb == 1);
        return HUFn_decompress4X2_DCtx_wksp(dctx, dst, dstSize, cSrc, cSrcSize, workSpace, wkspSize);
#else
        return algoNb ? HUFn_decompress4X2_DCtx_wksp(dctx, dst, dstSize, cSrc,
                            cSrcSize, workSpace, wkspSize):
                        HUFn_decompress4X1_DCtx_wksp(dctx, dst, dstSize, cSrc, cSrcSize, workSpace, wkspSize);
#endif
    }
}

size_t HUFn_decompress1X_DCtx_wksp(HUFn_DTable* dctx, void* dst, size_t dstSize,
                                  const void* cSrc, size_t cSrcSize,
                                  void* workSpace, size_t wkspSize)
{
    /* validation checks */
    if (dstSize == 0) return ERROR(dstSize_tooSmall);
    if (cSrcSize > dstSize) return ERROR(corruption_detected);   /* invalid */
    if (cSrcSize == dstSize) { ZSTDn_memcpy(dst, cSrc, dstSize); return dstSize; }   /* not compressed */
    if (cSrcSize == 1) { ZSTDn_memset(dst, *(const BYTE*)cSrc, dstSize); return dstSize; }   /* RLE */

    {   U32 const algoNb = HUFn_selectDecoder(dstSize, cSrcSize);
#if defined(HUFn_FORCE_DECOMPRESS_X1)
        (void)algoNb;
        assert(algoNb == 0);
        return HUFn_decompress1X1_DCtx_wksp(dctx, dst, dstSize, cSrc,
                                cSrcSize, workSpace, wkspSize);
#elif defined(HUFn_FORCE_DECOMPRESS_X2)
        (void)algoNb;
        assert(algoNb == 1);
        return HUFn_decompress1X2_DCtx_wksp(dctx, dst, dstSize, cSrc,
                                cSrcSize, workSpace, wkspSize);
#else
        return algoNb ? HUFn_decompress1X2_DCtx_wksp(dctx, dst, dstSize, cSrc,
                                cSrcSize, workSpace, wkspSize):
                        HUFn_decompress1X1_DCtx_wksp(dctx, dst, dstSize, cSrc,
                                cSrcSize, workSpace, wkspSize);
#endif
    }
}


size_t HUFn_decompress1X_usingDTable_bmi2(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFn_DTable* DTable, int bmi2)
{
    DTableDesc const dtd = HUFn_getDTableDesc(DTable);
#if defined(HUFn_FORCE_DECOMPRESS_X1)
    (void)dtd;
    assert(dtd.tableType == 0);
    return HUFn_decompress1X1_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, bmi2);
#elif defined(HUFn_FORCE_DECOMPRESS_X2)
    (void)dtd;
    assert(dtd.tableType == 1);
    return HUFn_decompress1X2_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, bmi2);
#else
    return dtd.tableType ? HUFn_decompress1X2_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, bmi2) :
                           HUFn_decompress1X1_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, bmi2);
#endif
}

#ifndef HUFn_FORCE_DECOMPRESS_X2
size_t HUFn_decompress1X1_DCtx_wksp_bmi2(HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize, int bmi2)
{
    const BYTE* ip = (const BYTE*) cSrc;

    size_t const hSize = HUFn_readDTableX1_wksp_bmi2(dctx, cSrc, cSrcSize, workSpace, wkspSize, bmi2);
    if (HUFn_isError(hSize)) return hSize;
    if (hSize >= cSrcSize) return ERROR(srcSize_wrong);
    ip += hSize; cSrcSize -= hSize;

    return HUFn_decompress1X1_usingDTable_internal(dst, dstSize, ip, cSrcSize, dctx, bmi2);
}
#endif

size_t HUFn_decompress4X_usingDTable_bmi2(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFn_DTable* DTable, int bmi2)
{
    DTableDesc const dtd = HUFn_getDTableDesc(DTable);
#if defined(HUFn_FORCE_DECOMPRESS_X1)
    (void)dtd;
    assert(dtd.tableType == 0);
    return HUFn_decompress4X1_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, bmi2);
#elif defined(HUFn_FORCE_DECOMPRESS_X2)
    (void)dtd;
    assert(dtd.tableType == 1);
    return HUFn_decompress4X2_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, bmi2);
#else
    return dtd.tableType ? HUFn_decompress4X2_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, bmi2) :
                           HUFn_decompress4X1_usingDTable_internal(dst, maxDstSize, cSrc, cSrcSize, DTable, bmi2);
#endif
}

size_t HUFn_decompress4X_hufOnly_wksp_bmi2(HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize, int bmi2)
{
    /* validation checks */
    if (dstSize == 0) return ERROR(dstSize_tooSmall);
    if (cSrcSize == 0) return ERROR(corruption_detected);

    {   U32 const algoNb = HUFn_selectDecoder(dstSize, cSrcSize);
#if defined(HUFn_FORCE_DECOMPRESS_X1)
        (void)algoNb;
        assert(algoNb == 0);
        return HUFn_decompress4X1_DCtx_wksp_bmi2(dctx, dst, dstSize, cSrc, cSrcSize, workSpace, wkspSize, bmi2);
#elif defined(HUFn_FORCE_DECOMPRESS_X2)
        (void)algoNb;
        assert(algoNb == 1);
        return HUFn_decompress4X2_DCtx_wksp_bmi2(dctx, dst, dstSize, cSrc, cSrcSize, workSpace, wkspSize, bmi2);
#else
        return algoNb ? HUFn_decompress4X2_DCtx_wksp_bmi2(dctx, dst, dstSize, cSrc, cSrcSize, workSpace, wkspSize, bmi2) :
                        HUFn_decompress4X1_DCtx_wksp_bmi2(dctx, dst, dstSize, cSrc, cSrcSize, workSpace, wkspSize, bmi2);
#endif
    }
}

