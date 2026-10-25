/* ******************************************************************
 * huff0 huffman codec,
 * part of Finite State Entropy library
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


#ifndef HUFn_H_298734234
#define HUFn_H_298734234

/* *** Dependencies *** */
#include "zstd_deps.h"    /* size_t */


/* *** library symbols visibility *** */
/* Note : when linking with -fvisibility=hidden on gcc, or by default on Visual,
 *        HUF symbols remain "private" (internal symbols for library only).
 *        Set macro FSEn_DLL_EXPORT to 1 if you want HUF symbols visible on DLL interface */
#if defined(FSEn_DLL_EXPORT) && (FSEn_DLL_EXPORT==1) && defined(__GNUC__) && (__GNUC__ >= 4)
#  define HUFn_PUBLIC_API __attribute__ ((visibility ("default")))
#elif defined(FSEn_DLL_EXPORT) && (FSEn_DLL_EXPORT==1)   /* Visual expected */
#  define HUFn_PUBLIC_API __declspec(dllexport)
#elif defined(FSEn_DLL_IMPORT) && (FSEn_DLL_IMPORT==1)
#  define HUFn_PUBLIC_API __declspec(dllimport)  /* not required, just to generate faster code (saves a function pointer load from IAT and an indirect jump) */
#else
#  define HUFn_PUBLIC_API
#endif


/* ========================== */
/* ***  simple functions  *** */
/* ========================== */

/* HUFn_compress() :
 *  Compress content from buffer 'src', of size 'srcSize', into buffer 'dst'.
 * 'dst' buffer must be already allocated.
 *  Compression runs faster if `dstCapacity` >= HUFn_compressBound(srcSize).
 * `srcSize` must be <= `HUFn_BLOCKSIZE_MAX` == 128 KB.
 * @return : size of compressed data (<= `dstCapacity`).
 *  Special values : if return == 0, srcData is not compressible => Nothing is stored within dst !!!
 *                   if HUFn_isError(return), compression failed (more details using HUFn_getErrorName())
 */
HUFn_PUBLIC_API size_t HUFn_compress(void* dst, size_t dstCapacity,
                             const void* src, size_t srcSize);

/* HUFn_decompress() :
 *  Decompress HUF data from buffer 'cSrc', of size 'cSrcSize',
 *  into already allocated buffer 'dst', of minimum size 'dstSize'.
 * `originalSize` : **must** be the ***exact*** size of original (uncompressed) data.
 *  Note : in contrast with FSE, HUFn_decompress can regenerate
 *         RLE (cSrcSize==1) and uncompressed (cSrcSize==dstSize) data,
 *         because it knows size to regenerate (originalSize).
 * @return : size of regenerated data (== originalSize),
 *           or an error code, which can be tested using HUFn_isError()
 */
HUFn_PUBLIC_API size_t HUFn_decompress(void* dst,  size_t originalSize,
                               const void* cSrc, size_t cSrcSize);


/* ***   Tool functions *** */
#define HUFn_BLOCKSIZE_MAX (128 * 1024)                  /*< maximum input size for a single block compressed with HUFn_compress */
HUFn_PUBLIC_API size_t HUFn_compressBound(size_t size);   /*< maximum compressed size (worst case) */

/* Error Management */
HUFn_PUBLIC_API unsigned    HUFn_isError(size_t code);       /*< tells if a return value is an error code */
HUFn_PUBLIC_API const char* HUFn_getErrorName(size_t code);  /*< provides error code string (useful for debugging) */


/* ***   Advanced function   *** */

/* HUFn_compress2() :
 *  Same as HUFn_compress(), but offers control over `maxSymbolValue` and `tableLog`.
 * `maxSymbolValue` must be <= HUFn_SYMBOLVALUE_MAX .
 * `tableLog` must be `<= HUFn_TABLELOG_MAX` . */
HUFn_PUBLIC_API size_t HUFn_compress2 (void* dst, size_t dstCapacity,
                               const void* src, size_t srcSize,
                               unsigned maxSymbolValue, unsigned tableLog);

/* HUFn_compress4X_wksp() :
 *  Same as HUFn_compress2(), but uses externally allocated `workSpace`.
 * `workspace` must have minimum alignment of 4, and be at least as large as HUFn_WORKSPACE_SIZE */
#define HUFn_WORKSPACE_SIZE ((6 << 10) + 256)
#define HUFn_WORKSPACE_SIZE_U32 (HUFn_WORKSPACE_SIZE / sizeof(U32))
HUFn_PUBLIC_API size_t HUFn_compress4X_wksp (void* dst, size_t dstCapacity,
                                     const void* src, size_t srcSize,
                                     unsigned maxSymbolValue, unsigned tableLog,
                                     void* workSpace, size_t wkspSize);

#endif   /* HUFn_H_298734234 */

/* ******************************************************************
 *  WARNING !!
 *  The following section contains advanced and experimental definitions
 *  which shall never be used in the context of a dynamic library,
 *  because they are not guaranteed to remain stable in the future.
 *  Only consider them in association with static linking.
 * *****************************************************************/
#if !defined(HUFn_H_HUF_STATIC_LINKING_ONLY)
#define HUFn_H_HUF_STATIC_LINKING_ONLY

/* *** Dependencies *** */
#include "mem.h"   /* U32 */
#define FSEn_STATIC_LINKING_ONLY
#include "fse.h"


/* *** Constants *** */
#define HUFn_TABLELOG_MAX      12      /* max runtime value of tableLog (due to static allocation); can be modified up to HUFn_ABSOLUTEMAX_TABLELOG */
#define HUFn_TABLELOG_DEFAULT  11      /* default tableLog value when none specified */
#define HUFn_SYMBOLVALUE_MAX  255

#define HUFn_TABLELOG_ABSOLUTEMAX  15  /* absolute limit of HUFn_MAX_TABLELOG. Beyond that value, code does not work */
#if (HUFn_TABLELOG_MAX > HUFn_TABLELOG_ABSOLUTEMAX)
#  error "HUFn_TABLELOG_MAX is too large !"
#endif


/* ****************************************
*  Static allocation
******************************************/
/* HUF buffer bounds */
#define HUFn_CTABLEBOUND 129
#define HUFn_BLOCKBOUND(size) (size + (size>>8) + 8)   /* only true when incompressible is pre-filtered with fast heuristic */
#define HUFn_COMPRESSBOUND(size) (HUFn_CTABLEBOUND + HUFn_BLOCKBOUND(size))   /* Macro version, useful for static allocation */

/* static allocation of HUF's Compression Table */
/* this is a private definition, just exposed for allocation and strict aliasing purpose. never EVER access its members directly */
struct HUFn_CElt_s {
  U16  val;
  BYTE nbBits;
};   /* typedef'd to HUFn_CElt */
typedef struct HUFn_CElt_s HUFn_CElt;   /* consider it an incomplete type */
#define HUFn_CTABLE_SIZE_U32(maxSymbolValue)   ((maxSymbolValue)+1)   /* Use tables of U32, for proper alignment */
#define HUFn_CTABLE_SIZE(maxSymbolValue)       (HUFn_CTABLE_SIZE_U32(maxSymbolValue) * sizeof(U32))
#define HUFn_CREATE_STATIC_CTABLE(name, maxSymbolValue) \
    HUFn_CElt name[HUFn_CTABLE_SIZE_U32(maxSymbolValue)] /* no final ; */

/* static allocation of HUF's DTable */
typedef U32 HUFn_DTable;
#define HUFn_DTABLE_SIZE(maxTableLog)   (1 + (1<<(maxTableLog)))
#define HUFn_CREATE_STATIC_DTABLEX1(DTable, maxTableLog) \
        HUFn_DTable DTable[HUFn_DTABLE_SIZE((maxTableLog)-1)] = { ((U32)((maxTableLog)-1) * 0x01000001) }
#define HUFn_CREATE_STATIC_DTABLEX2(DTable, maxTableLog) \
        HUFn_DTable DTable[HUFn_DTABLE_SIZE(maxTableLog)] = { ((U32)(maxTableLog) * 0x01000001) }


/* ****************************************
*  Advanced decompression functions
******************************************/
size_t HUFn_decompress4X1 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /*< single-symbol decoder */
#ifndef HUFn_FORCE_DECOMPRESS_X1
size_t HUFn_decompress4X2 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /*< double-symbols decoder */
#endif

size_t HUFn_decompress4X_DCtx (HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /*< decodes RLE and uncompressed */
size_t HUFn_decompress4X_hufOnly(HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize); /*< considers RLE and uncompressed as errors */
size_t HUFn_decompress4X_hufOnly_wksp(HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize); /*< considers RLE and uncompressed as errors */
size_t HUFn_decompress4X1_DCtx(HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /*< single-symbol decoder */
size_t HUFn_decompress4X1_DCtx_wksp(HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize);   /*< single-symbol decoder */
#ifndef HUFn_FORCE_DECOMPRESS_X1
size_t HUFn_decompress4X2_DCtx(HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /*< double-symbols decoder */
size_t HUFn_decompress4X2_DCtx_wksp(HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize);   /*< double-symbols decoder */
#endif


/* ****************************************
 *  HUF detailed API
 * ****************************************/

/*! HUFn_compress() does the following:
 *  1. count symbol occurrence from source[] into table count[] using FSEn_count() (exposed within "fse.h")
 *  2. (optional) refine tableLog using HUFn_optimalTableLog()
 *  3. build Huffman table from count using HUFn_buildCTable()
 *  4. save Huffman table to memory buffer using HUFn_writeCTable()
 *  5. encode the data stream using HUFn_compress4X_usingCTable()
 *
 *  The following API allows targeting specific sub-functions for advanced tasks.
 *  For example, it's possible to compress several blocks using the same 'CTable',
 *  or to save and regenerate 'CTable' using external methods.
 */
unsigned HUFn_optimalTableLog(unsigned maxTableLog, size_t srcSize, unsigned maxSymbolValue);
size_t HUFn_buildCTable (HUFn_CElt* CTable, const unsigned* count, unsigned maxSymbolValue, unsigned maxNbBits);   /* @return : maxNbBits; CTable and count can overlap. In which case, CTable will overwrite count content */
size_t HUFn_writeCTable (void* dst, size_t maxDstSize, const HUFn_CElt* CTable, unsigned maxSymbolValue, unsigned huffLog);
size_t HUFn_writeCTable_wksp(void* dst, size_t maxDstSize, const HUFn_CElt* CTable, unsigned maxSymbolValue, unsigned huffLog, void* workspace, size_t workspaceSize);
size_t HUFn_compress4X_usingCTable(void* dst, size_t dstSize, const void* src, size_t srcSize, const HUFn_CElt* CTable);
size_t HUFn_estimateCompressedSize(const HUFn_CElt* CTable, const unsigned* count, unsigned maxSymbolValue);
int HUFn_validateCTable(const HUFn_CElt* CTable, const unsigned* count, unsigned maxSymbolValue);

typedef enum {
   HUFn_repeat_none,  /*< Cannot use the previous table */
   HUFn_repeat_check, /*< Can use the previous table but it must be checked. Note : The previous table must have been constructed by HUFn_compress{1, 4}X_repeat */
   HUFn_repeat_valid  /*< Can use the previous table and it is assumed to be valid */
 } HUFn_repeat;
/* HUFn_compress4X_repeat() :
 *  Same as HUFn_compress4X_wksp(), but considers using hufTable if *repeat != HUFn_repeat_none.
 *  If it uses hufTable it does not modify hufTable or repeat.
 *  If it doesn't, it sets *repeat = HUFn_repeat_none, and it sets hufTable to the table used.
 *  If preferRepeat then the old table will always be used if valid. */
size_t HUFn_compress4X_repeat(void* dst, size_t dstSize,
                       const void* src, size_t srcSize,
                       unsigned maxSymbolValue, unsigned tableLog,
                       void* workSpace, size_t wkspSize,    /*< `workSpace` must be aligned on 4-bytes boundaries, `wkspSize` must be >= HUFn_WORKSPACE_SIZE */
                       HUFn_CElt* hufTable, HUFn_repeat* repeat, int preferRepeat, int bmi2);

/* HUFn_buildCTable_wksp() :
 *  Same as HUFn_buildCTable(), but using externally allocated scratch buffer.
 * `workSpace` must be aligned on 4-bytes boundaries, and its size must be >= HUFn_CTABLE_WORKSPACE_SIZE.
 */
#define HUFn_CTABLE_WORKSPACE_SIZE_U32 (2*HUFn_SYMBOLVALUE_MAX +1 +1)
#define HUFn_CTABLE_WORKSPACE_SIZE (HUFn_CTABLE_WORKSPACE_SIZE_U32 * sizeof(unsigned))
size_t HUFn_buildCTable_wksp (HUFn_CElt* tree,
                       const unsigned* count, U32 maxSymbolValue, U32 maxNbBits,
                             void* workSpace, size_t wkspSize);

/*! HUFn_readStats() :
 *  Read compact Huffman tree, saved by HUFn_writeCTable().
 * `huffWeight` is destination buffer.
 * @return : size read from `src` , or an error Code .
 *  Note : Needed by HUFn_readCTable() and HUFn_readDTableXn() . */
size_t HUFn_readStats(BYTE* huffWeight, size_t hwSize,
                     U32* rankStats, U32* nbSymbolsPtr, U32* tableLogPtr,
                     const void* src, size_t srcSize);

/*! HUFn_readStats_wksp() :
 * Same as HUFn_readStats() but takes an external workspace which must be
 * 4-byte aligned and its size must be >= HUFn_READ_STATS_WORKSPACE_SIZE.
 * If the CPU has BMI2 support, pass bmi2=1, otherwise pass bmi2=0.
 */
#define HUFn_READ_STATS_WORKSPACE_SIZE_U32 FSEn_DECOMPRESS_WKSP_SIZE_U32(6, HUFn_TABLELOG_MAX-1)
#define HUFn_READ_STATS_WORKSPACE_SIZE (HUFn_READ_STATS_WORKSPACE_SIZE_U32 * sizeof(unsigned))
size_t HUFn_readStats_wksp(BYTE* huffWeight, size_t hwSize,
                          U32* rankStats, U32* nbSymbolsPtr, U32* tableLogPtr,
                          const void* src, size_t srcSize,
                          void* workspace, size_t wkspSize,
                          int bmi2);

/* HUFn_readCTable() :
 *  Loading a CTable saved with HUFn_writeCTable() */
size_t HUFn_readCTable (HUFn_CElt* CTable, unsigned* maxSymbolValuePtr, const void* src, size_t srcSize, unsigned *hasZeroWeights);

/* HUFn_getNbBits() :
 *  Read nbBits from CTable symbolTable, for symbol `symbolValue` presumed <= HUFn_SYMBOLVALUE_MAX
 *  Note 1 : is not inlined, as HUFn_CElt definition is private
 *  Note 2 : const void* used, so that it can provide a statically allocated table as argument (which uses type U32) */
U32 HUFn_getNbBits(const void* symbolTable, U32 symbolValue);

/*
 * HUFn_decompress() does the following:
 * 1. select the decompression algorithm (X1, X2) based on pre-computed heuristics
 * 2. build Huffman table from save, using HUFn_readDTableX?()
 * 3. decode 1 or 4 segments in parallel using HUFn_decompress?X?_usingDTable()
 */

/* HUFn_selectDecoder() :
 *  Tells which decoder is likely to decode faster,
 *  based on a set of pre-computed metrics.
 * @return : 0==HUFn_decompress4X1, 1==HUFn_decompress4X2 .
 *  Assumption : 0 < dstSize <= 128 KB */
U32 HUFn_selectDecoder (size_t dstSize, size_t cSrcSize);

/*
 *  The minimum workspace size for the `workSpace` used in
 *  HUFn_readDTableX1_wksp() and HUFn_readDTableX2_wksp().
 *
 *  The space used depends on HUFn_TABLELOG_MAX, ranging from ~1500 bytes when
 *  HUFn_TABLE_LOG_MAX=12 to ~1850 bytes when HUFn_TABLE_LOG_MAX=15.
 *  Buffer overflow errors may potentially occur if code modifications result in
 *  a required workspace size greater than that specified in the following
 *  macro.
 */
#define HUFn_DECOMPRESS_WORKSPACE_SIZE ((2 << 10) + (1 << 9))
#define HUFn_DECOMPRESS_WORKSPACE_SIZE_U32 (HUFn_DECOMPRESS_WORKSPACE_SIZE / sizeof(U32))

#ifndef HUFn_FORCE_DECOMPRESS_X2
size_t HUFn_readDTableX1 (HUFn_DTable* DTable, const void* src, size_t srcSize);
size_t HUFn_readDTableX1_wksp (HUFn_DTable* DTable, const void* src, size_t srcSize, void* workSpace, size_t wkspSize);
#endif
#ifndef HUFn_FORCE_DECOMPRESS_X1
size_t HUFn_readDTableX2 (HUFn_DTable* DTable, const void* src, size_t srcSize);
size_t HUFn_readDTableX2_wksp (HUFn_DTable* DTable, const void* src, size_t srcSize, void* workSpace, size_t wkspSize);
#endif

size_t HUFn_decompress4X_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFn_DTable* DTable);
#ifndef HUFn_FORCE_DECOMPRESS_X2
size_t HUFn_decompress4X1_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFn_DTable* DTable);
#endif
#ifndef HUFn_FORCE_DECOMPRESS_X1
size_t HUFn_decompress4X2_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFn_DTable* DTable);
#endif


/* ====================== */
/* single stream variants */
/* ====================== */

size_t HUFn_compress1X (void* dst, size_t dstSize, const void* src, size_t srcSize, unsigned maxSymbolValue, unsigned tableLog);
size_t HUFn_compress1X_wksp (void* dst, size_t dstSize, const void* src, size_t srcSize, unsigned maxSymbolValue, unsigned tableLog, void* workSpace, size_t wkspSize);  /*< `workSpace` must be a table of at least HUFn_WORKSPACE_SIZE_U32 unsigned */
size_t HUFn_compress1X_usingCTable(void* dst, size_t dstSize, const void* src, size_t srcSize, const HUFn_CElt* CTable);
/* HUFn_compress1X_repeat() :
 *  Same as HUFn_compress1X_wksp(), but considers using hufTable if *repeat != HUFn_repeat_none.
 *  If it uses hufTable it does not modify hufTable or repeat.
 *  If it doesn't, it sets *repeat = HUFn_repeat_none, and it sets hufTable to the table used.
 *  If preferRepeat then the old table will always be used if valid. */
size_t HUFn_compress1X_repeat(void* dst, size_t dstSize,
                       const void* src, size_t srcSize,
                       unsigned maxSymbolValue, unsigned tableLog,
                       void* workSpace, size_t wkspSize,   /*< `workSpace` must be aligned on 4-bytes boundaries, `wkspSize` must be >= HUFn_WORKSPACE_SIZE */
                       HUFn_CElt* hufTable, HUFn_repeat* repeat, int preferRepeat, int bmi2);

size_t HUFn_decompress1X1 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /* single-symbol decoder */
#ifndef HUFn_FORCE_DECOMPRESS_X1
size_t HUFn_decompress1X2 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /* double-symbol decoder */
#endif

size_t HUFn_decompress1X_DCtx (HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);
size_t HUFn_decompress1X_DCtx_wksp (HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize);
#ifndef HUFn_FORCE_DECOMPRESS_X2
size_t HUFn_decompress1X1_DCtx(HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /*< single-symbol decoder */
size_t HUFn_decompress1X1_DCtx_wksp(HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize);   /*< single-symbol decoder */
#endif
#ifndef HUFn_FORCE_DECOMPRESS_X1
size_t HUFn_decompress1X2_DCtx(HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /*< double-symbols decoder */
size_t HUFn_decompress1X2_DCtx_wksp(HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize);   /*< double-symbols decoder */
#endif

size_t HUFn_decompress1X_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFn_DTable* DTable);   /*< automatic selection of sing or double symbol decoder, based on DTable */
#ifndef HUFn_FORCE_DECOMPRESS_X2
size_t HUFn_decompress1X1_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFn_DTable* DTable);
#endif
#ifndef HUFn_FORCE_DECOMPRESS_X1
size_t HUFn_decompress1X2_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFn_DTable* DTable);
#endif

/* BMI2 variants.
 * If the CPU has BMI2 support, pass bmi2=1, otherwise pass bmi2=0.
 */
size_t HUFn_decompress1X_usingDTable_bmi2(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFn_DTable* DTable, int bmi2);
#ifndef HUFn_FORCE_DECOMPRESS_X2
size_t HUFn_decompress1X1_DCtx_wksp_bmi2(HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize, int bmi2);
#endif
size_t HUFn_decompress4X_usingDTable_bmi2(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUFn_DTable* DTable, int bmi2);
size_t HUFn_decompress4X_hufOnly_wksp_bmi2(HUFn_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize, int bmi2);
#ifndef HUFn_FORCE_DECOMPRESS_X2
size_t HUFn_readDTableX1_wksp_bmi2(HUFn_DTable* DTable, const void* src, size_t srcSize, void* workSpace, size_t wkspSize, int bmi2);
#endif

#endif /* HUFn_STATIC_LINKING_ONLY */

