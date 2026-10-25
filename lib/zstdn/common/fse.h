/* ******************************************************************
 * FSE : Finite State Entropy codec
 * Public Prototypes declaration
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


#ifndef FSEn_H
#define FSEn_H


/*-*****************************************
*  Dependencies
******************************************/
#include "zstd_deps.h"    /* size_t, ptrdiff_t */


/*-*****************************************
*  FSEn_PUBLIC_API : control library symbols visibility
******************************************/
#if defined(FSEn_DLL_EXPORT) && (FSEn_DLL_EXPORT==1) && defined(__GNUC__) && (__GNUC__ >= 4)
#  define FSEn_PUBLIC_API __attribute__ ((visibility ("default")))
#elif defined(FSEn_DLL_EXPORT) && (FSEn_DLL_EXPORT==1)   /* Visual expected */
#  define FSEn_PUBLIC_API __declspec(dllexport)
#elif defined(FSEn_DLL_IMPORT) && (FSEn_DLL_IMPORT==1)
#  define FSEn_PUBLIC_API __declspec(dllimport) /* It isn't required but allows to generate better code, saving a function pointer load from the IAT and an indirect jump.*/
#else
#  define FSEn_PUBLIC_API
#endif

/*------   Version   ------*/
#define FSEn_VERSION_MAJOR    0
#define FSEn_VERSION_MINOR    9
#define FSEn_VERSION_RELEASE  0

#define FSEn_LIB_VERSION FSEn_VERSION_MAJOR.FSEn_VERSION_MINOR.FSEn_VERSION_RELEASE
#define FSEn_QUOTE(str) #str
#define FSEn_EXPAND_AND_QUOTE(str) FSEn_QUOTE(str)
#define FSEn_VERSION_STRING FSEn_EXPAND_AND_QUOTE(FSEn_LIB_VERSION)

#define FSEn_VERSION_NUMBER  (FSEn_VERSION_MAJOR *100*100 + FSEn_VERSION_MINOR *100 + FSEn_VERSION_RELEASE)
FSEn_PUBLIC_API unsigned FSEn_versionNumber(void);   /*< library version number; to be used when checking dll version */


/*-****************************************
*  FSE simple functions
******************************************/
/*! FSEn_compress() :
    Compress content of buffer 'src', of size 'srcSize', into destination buffer 'dst'.
    'dst' buffer must be already allocated. Compression runs faster is dstCapacity >= FSEn_compressBound(srcSize).
    @return : size of compressed data (<= dstCapacity).
    Special values : if return == 0, srcData is not compressible => Nothing is stored within dst !!!
                     if return == 1, srcData is a single byte symbol * srcSize times. Use RLE compression instead.
                     if FSEn_isError(return), compression failed (more details using FSEn_getErrorName())
*/
FSEn_PUBLIC_API size_t FSEn_compress(void* dst, size_t dstCapacity,
                             const void* src, size_t srcSize);

/*! FSEn_decompress():
    Decompress FSE data from buffer 'cSrc', of size 'cSrcSize',
    into already allocated destination buffer 'dst', of size 'dstCapacity'.
    @return : size of regenerated data (<= maxDstSize),
              or an error code, which can be tested using FSEn_isError() .

    ** Important ** : FSEn_decompress() does not decompress non-compressible nor RLE data !!!
    Why ? : making this distinction requires a header.
    Header management is intentionally delegated to the user layer, which can better manage special cases.
*/
FSEn_PUBLIC_API size_t FSEn_decompress(void* dst,  size_t dstCapacity,
                               const void* cSrc, size_t cSrcSize);


/*-*****************************************
*  Tool functions
******************************************/
FSEn_PUBLIC_API size_t FSEn_compressBound(size_t size);       /* maximum compressed size */

/* Error Management */
FSEn_PUBLIC_API unsigned    FSEn_isError(size_t code);        /* tells if a return value is an error code */
FSEn_PUBLIC_API const char* FSEn_getErrorName(size_t code);   /* provides error code string (useful for debugging) */


/*-*****************************************
*  FSE advanced functions
******************************************/
/*! FSEn_compress2() :
    Same as FSEn_compress(), but allows the selection of 'maxSymbolValue' and 'tableLog'
    Both parameters can be defined as '0' to mean : use default value
    @return : size of compressed data
    Special values : if return == 0, srcData is not compressible => Nothing is stored within cSrc !!!
                     if return == 1, srcData is a single byte symbol * srcSize times. Use RLE compression.
                     if FSEn_isError(return), it's an error code.
*/
FSEn_PUBLIC_API size_t FSEn_compress2 (void* dst, size_t dstSize, const void* src, size_t srcSize, unsigned maxSymbolValue, unsigned tableLog);


/*-*****************************************
*  FSE detailed API
******************************************/
/*!
FSEn_compress() does the following:
1. count symbol occurrence from source[] into table count[] (see hist.h)
2. normalize counters so that sum(count[]) == Power_of_2 (2^tableLog)
3. save normalized counters to memory buffer using writeNCount()
4. build encoding table 'CTable' from normalized counters
5. encode the data stream using encoding table 'CTable'

FSEn_decompress() does the following:
1. read normalized counters with readNCount()
2. build decoding table 'DTable' from normalized counters
3. decode the data stream using decoding table 'DTable'

The following API allows targeting specific sub-functions for advanced tasks.
For example, it's possible to compress several blocks using the same 'CTable',
or to save and provide normalized distribution using external method.
*/

/* *** COMPRESSION *** */

/*! FSEn_optimalTableLog():
    dynamically downsize 'tableLog' when conditions are met.
    It saves CPU time, by using smaller tables, while preserving or even improving compression ratio.
    @return : recommended tableLog (necessarily <= 'maxTableLog') */
FSEn_PUBLIC_API unsigned FSEn_optimalTableLog(unsigned maxTableLog, size_t srcSize, unsigned maxSymbolValue);

/*! FSEn_normalizeCount():
    normalize counts so that sum(count[]) == Power_of_2 (2^tableLog)
    'normalizedCounter' is a table of short, of minimum size (maxSymbolValue+1).
    useLowProbCount is a boolean parameter which trades off compressed size for
    faster header decoding. When it is set to 1, the compressed data will be slightly
    smaller. And when it is set to 0, FSEn_readNCount() and FSEn_buildDTable() will be
    faster. If you are compressing a small amount of data (< 2 KB) then useLowProbCount=0
    is a good default, since header deserialization makes a big speed difference.
    Otherwise, useLowProbCount=1 is a good default, since the speed difference is small.
    @return : tableLog,
              or an errorCode, which can be tested using FSEn_isError() */
FSEn_PUBLIC_API size_t FSEn_normalizeCount(short* normalizedCounter, unsigned tableLog,
                    const unsigned* count, size_t srcSize, unsigned maxSymbolValue, unsigned useLowProbCount);

/*! FSEn_NCountWriteBound():
    Provides the maximum possible size of an FSE normalized table, given 'maxSymbolValue' and 'tableLog'.
    Typically useful for allocation purpose. */
FSEn_PUBLIC_API size_t FSEn_NCountWriteBound(unsigned maxSymbolValue, unsigned tableLog);

/*! FSEn_writeNCount():
    Compactly save 'normalizedCounter' into 'buffer'.
    @return : size of the compressed table,
              or an errorCode, which can be tested using FSEn_isError(). */
FSEn_PUBLIC_API size_t FSEn_writeNCount (void* buffer, size_t bufferSize,
                                 const short* normalizedCounter,
                                 unsigned maxSymbolValue, unsigned tableLog);

/*! Constructor and Destructor of FSEn_CTable.
    Note that FSEn_CTable size depends on 'tableLog' and 'maxSymbolValue' */
typedef unsigned FSEn_CTable;   /* don't allocate that. It's only meant to be more restrictive than void* */
FSEn_PUBLIC_API FSEn_CTable* FSEn_createCTable (unsigned maxSymbolValue, unsigned tableLog);
FSEn_PUBLIC_API void        FSEn_freeCTable (FSEn_CTable* ct);

/*! FSEn_buildCTable():
    Builds `ct`, which must be already allocated, using FSEn_createCTable().
    @return : 0, or an errorCode, which can be tested using FSEn_isError() */
FSEn_PUBLIC_API size_t FSEn_buildCTable(FSEn_CTable* ct, const short* normalizedCounter, unsigned maxSymbolValue, unsigned tableLog);

/*! FSEn_compress_usingCTable():
    Compress `src` using `ct` into `dst` which must be already allocated.
    @return : size of compressed data (<= `dstCapacity`),
              or 0 if compressed data could not fit into `dst`,
              or an errorCode, which can be tested using FSEn_isError() */
FSEn_PUBLIC_API size_t FSEn_compress_usingCTable (void* dst, size_t dstCapacity, const void* src, size_t srcSize, const FSEn_CTable* ct);

/*!
Tutorial :
----------
The first step is to count all symbols. FSEn_count() does this job very fast.
Result will be saved into 'count', a table of unsigned int, which must be already allocated, and have 'maxSymbolValuePtr[0]+1' cells.
'src' is a table of bytes of size 'srcSize'. All values within 'src' MUST be <= maxSymbolValuePtr[0]
maxSymbolValuePtr[0] will be updated, with its real value (necessarily <= original value)
FSEn_count() will return the number of occurrence of the most frequent symbol.
This can be used to know if there is a single symbol within 'src', and to quickly evaluate its compressibility.
If there is an error, the function will return an ErrorCode (which can be tested using FSEn_isError()).

The next step is to normalize the frequencies.
FSEn_normalizeCount() will ensure that sum of frequencies is == 2 ^'tableLog'.
It also guarantees a minimum of 1 to any Symbol with frequency >= 1.
You can use 'tableLog'==0 to mean "use default tableLog value".
If you are unsure of which tableLog value to use, you can ask FSEn_optimalTableLog(),
which will provide the optimal valid tableLog given sourceSize, maxSymbolValue, and a user-defined maximum (0 means "default").

The result of FSEn_normalizeCount() will be saved into a table,
called 'normalizedCounter', which is a table of signed short.
'normalizedCounter' must be already allocated, and have at least 'maxSymbolValue+1' cells.
The return value is tableLog if everything proceeded as expected.
It is 0 if there is a single symbol within distribution.
If there is an error (ex: invalid tableLog value), the function will return an ErrorCode (which can be tested using FSEn_isError()).

'normalizedCounter' can be saved in a compact manner to a memory area using FSEn_writeNCount().
'buffer' must be already allocated.
For guaranteed success, buffer size must be at least FSEn_headerBound().
The result of the function is the number of bytes written into 'buffer'.
If there is an error, the function will return an ErrorCode (which can be tested using FSEn_isError(); ex : buffer size too small).

'normalizedCounter' can then be used to create the compression table 'CTable'.
The space required by 'CTable' must be already allocated, using FSEn_createCTable().
You can then use FSEn_buildCTable() to fill 'CTable'.
If there is an error, both functions will return an ErrorCode (which can be tested using FSEn_isError()).

'CTable' can then be used to compress 'src', with FSEn_compress_usingCTable().
Similar to FSEn_count(), the convention is that 'src' is assumed to be a table of char of size 'srcSize'
The function returns the size of compressed data (without header), necessarily <= `dstCapacity`.
If it returns '0', compressed data could not fit into 'dst'.
If there is an error, the function will return an ErrorCode (which can be tested using FSEn_isError()).
*/


/* *** DECOMPRESSION *** */

/*! FSEn_readNCount():
    Read compactly saved 'normalizedCounter' from 'rBuffer'.
    @return : size read from 'rBuffer',
              or an errorCode, which can be tested using FSEn_isError().
              maxSymbolValuePtr[0] and tableLogPtr[0] will also be updated with their respective values */
FSEn_PUBLIC_API size_t FSEn_readNCount (short* normalizedCounter,
                           unsigned* maxSymbolValuePtr, unsigned* tableLogPtr,
                           const void* rBuffer, size_t rBuffSize);

/*! FSEn_readNCount_bmi2():
 * Same as FSEn_readNCount() but pass bmi2=1 when your CPU supports BMI2 and 0 otherwise.
 */
FSEn_PUBLIC_API size_t FSEn_readNCount_bmi2(short* normalizedCounter,
                           unsigned* maxSymbolValuePtr, unsigned* tableLogPtr,
                           const void* rBuffer, size_t rBuffSize, int bmi2);

/*! Constructor and Destructor of FSEn_DTable.
    Note that its size depends on 'tableLog' */
typedef unsigned FSEn_DTable;   /* don't allocate that. It's just a way to be more restrictive than void* */
FSEn_PUBLIC_API FSEn_DTable* FSEn_createDTable(unsigned tableLog);
FSEn_PUBLIC_API void        FSEn_freeDTable(FSEn_DTable* dt);

/*! FSEn_buildDTable():
    Builds 'dt', which must be already allocated, using FSEn_createDTable().
    return : 0, or an errorCode, which can be tested using FSEn_isError() */
FSEn_PUBLIC_API size_t FSEn_buildDTable (FSEn_DTable* dt, const short* normalizedCounter, unsigned maxSymbolValue, unsigned tableLog);

/*! FSEn_decompress_usingDTable():
    Decompress compressed source `cSrc` of size `cSrcSize` using `dt`
    into `dst` which must be already allocated.
    @return : size of regenerated data (necessarily <= `dstCapacity`),
              or an errorCode, which can be tested using FSEn_isError() */
FSEn_PUBLIC_API size_t FSEn_decompress_usingDTable(void* dst, size_t dstCapacity, const void* cSrc, size_t cSrcSize, const FSEn_DTable* dt);

/*!
Tutorial :
----------
(Note : these functions only decompress FSE-compressed blocks.
 If block is uncompressed, use memcpy() instead
 If block is a single repeated byte, use memset() instead )

The first step is to obtain the normalized frequencies of symbols.
This can be performed by FSEn_readNCount() if it was saved using FSEn_writeNCount().
'normalizedCounter' must be already allocated, and have at least 'maxSymbolValuePtr[0]+1' cells of signed short.
In practice, that means it's necessary to know 'maxSymbolValue' beforehand,
or size the table to handle worst case situations (typically 256).
FSEn_readNCount() will provide 'tableLog' and 'maxSymbolValue'.
The result of FSEn_readNCount() is the number of bytes read from 'rBuffer'.
Note that 'rBufferSize' must be at least 4 bytes, even if useful information is less than that.
If there is an error, the function will return an error code, which can be tested using FSEn_isError().

The next step is to build the decompression tables 'FSEn_DTable' from 'normalizedCounter'.
This is performed by the function FSEn_buildDTable().
The space required by 'FSEn_DTable' must be already allocated using FSEn_createDTable().
If there is an error, the function will return an error code, which can be tested using FSEn_isError().

`FSEn_DTable` can then be used to decompress `cSrc`, with FSEn_decompress_usingDTable().
`cSrcSize` must be strictly correct, otherwise decompression will fail.
FSEn_decompress_usingDTable() result will tell how many bytes were regenerated (<=`dstCapacity`).
If there is an error, the function will return an error code, which can be tested using FSEn_isError(). (ex: dst buffer too small)
*/

#endif  /* FSEn_H */

#if !defined(FSEn_H_FSE_STATIC_LINKING_ONLY)
#define FSEn_H_FSE_STATIC_LINKING_ONLY

/* *** Dependency *** */
#include "bitstream.h"


/* *****************************************
*  Static allocation
*******************************************/
/* FSE buffer bounds */
#define FSEn_NCOUNTBOUND 512
#define FSEn_BLOCKBOUND(size) ((size) + ((size)>>7) + 4 /* fse states */ + sizeof(size_t) /* bitContainer */)
#define FSEn_COMPRESSBOUND(size) (FSEn_NCOUNTBOUND + FSEn_BLOCKBOUND(size))   /* Macro version, useful for static allocation */

/* It is possible to statically allocate FSE CTable/DTable as a table of FSEn_CTable/FSEn_DTable using below macros */
#define FSEn_CTABLE_SIZE_U32(maxTableLog, maxSymbolValue)   (1 + (1<<((maxTableLog)-1)) + (((maxSymbolValue)+1)*2))
#define FSEn_DTABLE_SIZE_U32(maxTableLog)                   (1 + (1<<(maxTableLog)))

/* or use the size to malloc() space directly. Pay attention to alignment restrictions though */
#define FSEn_CTABLE_SIZE(maxTableLog, maxSymbolValue)   (FSEn_CTABLE_SIZE_U32(maxTableLog, maxSymbolValue) * sizeof(FSEn_CTable))
#define FSEn_DTABLE_SIZE(maxTableLog)                   (FSEn_DTABLE_SIZE_U32(maxTableLog) * sizeof(FSEn_DTable))


/* *****************************************
 *  FSE advanced API
 ***************************************** */

unsigned FSEn_optimalTableLog_internal(unsigned maxTableLog, size_t srcSize, unsigned maxSymbolValue, unsigned minus);
/*< same as FSEn_optimalTableLog(), which used `minus==2` */

/* FSEn_compress_wksp() :
 * Same as FSEn_compress2(), but using an externally allocated scratch buffer (`workSpace`).
 * FSEn_COMPRESS_WKSP_SIZE_U32() provides the minimum size required for `workSpace` as a table of FSEn_CTable.
 */
#define FSEn_COMPRESS_WKSP_SIZE_U32(maxTableLog, maxSymbolValue)   ( FSEn_CTABLE_SIZE_U32(maxTableLog, maxSymbolValue) + ((maxTableLog > 12) ? (1 << (maxTableLog - 2)) : 1024) )
size_t FSEn_compress_wksp (void* dst, size_t dstSize, const void* src, size_t srcSize, unsigned maxSymbolValue, unsigned tableLog, void* workSpace, size_t wkspSize);

size_t FSEn_buildCTable_raw (FSEn_CTable* ct, unsigned nbBits);
/*< build a fake FSEn_CTable, designed for a flat distribution, where each symbol uses nbBits */

size_t FSEn_buildCTable_rle (FSEn_CTable* ct, unsigned char symbolValue);
/*< build a fake FSEn_CTable, designed to compress always the same symbolValue */

/* FSEn_buildCTable_wksp() :
 * Same as FSEn_buildCTable(), but using an externally allocated scratch buffer (`workSpace`).
 * `wkspSize` must be >= `FSEn_BUILD_CTABLE_WORKSPACE_SIZE_U32(maxSymbolValue, tableLog)` of `unsigned`.
 */
#define FSEn_BUILD_CTABLE_WORKSPACE_SIZE_U32(maxSymbolValue, tableLog) (maxSymbolValue + 2 + (1ull << (tableLog - 2)))
#define FSEn_BUILD_CTABLE_WORKSPACE_SIZE(maxSymbolValue, tableLog) (sizeof(unsigned) * FSEn_BUILD_CTABLE_WORKSPACE_SIZE_U32(maxSymbolValue, tableLog))
size_t FSEn_buildCTable_wksp(FSEn_CTable* ct, const short* normalizedCounter, unsigned maxSymbolValue, unsigned tableLog, void* workSpace, size_t wkspSize);

#define FSEn_BUILD_DTABLE_WKSP_SIZE(maxTableLog, maxSymbolValue) (sizeof(short) * (maxSymbolValue + 1) + (1ULL << maxTableLog) + 8)
#define FSEn_BUILD_DTABLE_WKSP_SIZE_U32(maxTableLog, maxSymbolValue) ((FSEn_BUILD_DTABLE_WKSP_SIZE(maxTableLog, maxSymbolValue) + sizeof(unsigned) - 1) / sizeof(unsigned))
FSEn_PUBLIC_API size_t FSEn_buildDTable_wksp(FSEn_DTable* dt, const short* normalizedCounter, unsigned maxSymbolValue, unsigned tableLog, void* workSpace, size_t wkspSize);
/*< Same as FSEn_buildDTable(), using an externally allocated `workspace` produced with `FSEn_BUILD_DTABLE_WKSP_SIZE_U32(maxSymbolValue)` */

size_t FSEn_buildDTable_raw (FSEn_DTable* dt, unsigned nbBits);
/*< build a fake FSEn_DTable, designed to read a flat distribution where each symbol uses nbBits */

size_t FSEn_buildDTable_rle (FSEn_DTable* dt, unsigned char symbolValue);
/*< build a fake FSEn_DTable, designed to always generate the same symbolValue */

#define FSEn_DECOMPRESS_WKSP_SIZE_U32(maxTableLog, maxSymbolValue) (FSEn_DTABLE_SIZE_U32(maxTableLog) + FSEn_BUILD_DTABLE_WKSP_SIZE_U32(maxTableLog, maxSymbolValue) + (FSEn_MAX_SYMBOL_VALUE + 1) / 2 + 1)
#define FSEn_DECOMPRESS_WKSP_SIZE(maxTableLog, maxSymbolValue) (FSEn_DECOMPRESS_WKSP_SIZE_U32(maxTableLog, maxSymbolValue) * sizeof(unsigned))
size_t FSEn_decompress_wksp(void* dst, size_t dstCapacity, const void* cSrc, size_t cSrcSize, unsigned maxLog, void* workSpace, size_t wkspSize);
/*< same as FSEn_decompress(), using an externally allocated `workSpace` produced with `FSEn_DECOMPRESS_WKSP_SIZE_U32(maxLog, maxSymbolValue)` */

size_t FSEn_decompress_wksp_bmi2(void* dst, size_t dstCapacity, const void* cSrc, size_t cSrcSize, unsigned maxLog, void* workSpace, size_t wkspSize, int bmi2);
/*< Same as FSEn_decompress_wksp() but with dynamic BMI2 support. Pass 1 if your CPU supports BMI2 or 0 if it doesn't. */

typedef enum {
   FSEn_repeat_none,  /*< Cannot use the previous table */
   FSEn_repeat_check, /*< Can use the previous table but it must be checked */
   FSEn_repeat_valid  /*< Can use the previous table and it is assumed to be valid */
 } FSEn_repeat;

/* *****************************************
*  FSE symbol compression API
*******************************************/
/*!
   This API consists of small unitary functions, which highly benefit from being inlined.
   Hence their body are included in next section.
*/
typedef struct {
    ptrdiff_t   value;
    const void* stateTable;
    const void* symbolTT;
    unsigned    stateLog;
} FSEn_CState_t;

static void FSEn_initCState(FSEn_CState_t* CStatePtr, const FSEn_CTable* ct);

static void FSEn_encodeSymbol(BITn_CStream_t* bitC, FSEn_CState_t* CStatePtr, unsigned symbol);

static void FSEn_flushCState(BITn_CStream_t* bitC, const FSEn_CState_t* CStatePtr);

/*<
These functions are inner components of FSEn_compress_usingCTable().
They allow the creation of custom streams, mixing multiple tables and bit sources.

A key property to keep in mind is that encoding and decoding are done **in reverse direction**.
So the first symbol you will encode is the last you will decode, like a LIFO stack.

You will need a few variables to track your CStream. They are :

FSEn_CTable    ct;         // Provided by FSEn_buildCTable()
BITn_CStream_t bitStream;  // bitStream tracking structure
FSEn_CState_t  state;      // State tracking structure (can have several)


The first thing to do is to init bitStream and state.
    size_t errorCode = BITn_initCStream(&bitStream, dstBuffer, maxDstSize);
    FSEn_initCState(&state, ct);

Note that BITn_initCStream() can produce an error code, so its result should be tested, using FSEn_isError();
You can then encode your input data, byte after byte.
FSEn_encodeSymbol() outputs a maximum of 'tableLog' bits at a time.
Remember decoding will be done in reverse direction.
    FSEn_encodeByte(&bitStream, &state, symbol);

At any time, you can also add any bit sequence.
Note : maximum allowed nbBits is 25, for compatibility with 32-bits decoders
    BITn_addBits(&bitStream, bitField, nbBits);

The above methods don't commit data to memory, they just store it into local register, for speed.
Local register size is 64-bits on 64-bits systems, 32-bits on 32-bits systems (size_t).
Writing data to memory is a manual operation, performed by the flushBits function.
    BITn_flushBits(&bitStream);

Your last FSE encoding operation shall be to flush your last state value(s).
    FSEn_flushState(&bitStream, &state);

Finally, you must close the bitStream.
The function returns the size of CStream in bytes.
If data couldn't fit into dstBuffer, it will return a 0 ( == not compressible)
If there is an error, it returns an errorCode (which can be tested using FSEn_isError()).
    size_t size = BITn_closeCStream(&bitStream);
*/


/* *****************************************
*  FSE symbol decompression API
*******************************************/
typedef struct {
    size_t      state;
    const void* table;   /* precise table may vary, depending on U16 */
} FSEn_DState_t;


static void     FSEn_initDState(FSEn_DState_t* DStatePtr, BITn_DStream_t* bitD, const FSEn_DTable* dt);

static unsigned char FSEn_decodeSymbol(FSEn_DState_t* DStatePtr, BITn_DStream_t* bitD);

static unsigned FSEn_endOfDState(const FSEn_DState_t* DStatePtr);

/*<
Let's now decompose FSEn_decompress_usingDTable() into its unitary components.
You will decode FSE-encoded symbols from the bitStream,
and also any other bitFields you put in, **in reverse order**.

You will need a few variables to track your bitStream. They are :

BITn_DStream_t DStream;    // Stream context
FSEn_DState_t  DState;     // State context. Multiple ones are possible
FSEn_DTable*   DTablePtr;  // Decoding table, provided by FSEn_buildDTable()

The first thing to do is to init the bitStream.
    errorCode = BITn_initDStream(&DStream, srcBuffer, srcSize);

You should then retrieve your initial state(s)
(in reverse flushing order if you have several ones) :
    errorCode = FSEn_initDState(&DState, &DStream, DTablePtr);

You can then decode your data, symbol after symbol.
For information the maximum number of bits read by FSEn_decodeSymbol() is 'tableLog'.
Keep in mind that symbols are decoded in reverse order, like a LIFO stack (last in, first out).
    unsigned char symbol = FSEn_decodeSymbol(&DState, &DStream);

You can retrieve any bitfield you eventually stored into the bitStream (in reverse order)
Note : maximum allowed nbBits is 25, for 32-bits compatibility
    size_t bitField = BITn_readBits(&DStream, nbBits);

All above operations only read from local register (which size depends on size_t).
Refueling the register from memory is manually performed by the reload method.
    endSignal = FSEn_reloadDStream(&DStream);

BITn_reloadDStream() result tells if there is still some more data to read from DStream.
BITn_DStream_unfinished : there is still some data left into the DStream.
BITn_DStream_endOfBuffer : Dstream reached end of buffer. Its container may no longer be completely filled.
BITn_DStream_completed : Dstream reached its exact end, corresponding in general to decompression completed.
BITn_DStream_tooFar : Dstream went too far. Decompression result is corrupted.

When reaching end of buffer (BITn_DStream_endOfBuffer), progress slowly, notably if you decode multiple symbols per loop,
to properly detect the exact end of stream.
After each decoded symbol, check if DStream is fully consumed using this simple test :
    BITn_reloadDStream(&DStream) >= BITn_DStream_completed

When it's done, verify decompression is fully completed, by checking both DStream and the relevant states.
Checking if DStream has reached its end is performed by :
    BITn_endOfDStream(&DStream);
Check also the states. There might be some symbols left there, if some high probability ones (>50%) are possible.
    FSEn_endOfDState(&DState);
*/


/* *****************************************
*  FSE unsafe API
*******************************************/
static unsigned char FSEn_decodeSymbolFast(FSEn_DState_t* DStatePtr, BITn_DStream_t* bitD);
/* faster, but works only if nbBits is always >= 1 (otherwise, result will be corrupted) */


/* *****************************************
*  Implementation of inlined functions
*******************************************/
typedef struct {
    int deltaFindState;
    U32 deltaNbBits;
} FSEn_symbolCompressionTransform; /* total 8 bytes */

MEMn_STATIC void FSEn_initCState(FSEn_CState_t* statePtr, const FSEn_CTable* ct)
{
    const void* ptr = ct;
    const U16* u16ptr = (const U16*) ptr;
    const U32 tableLog = MEMn_read16(ptr);
    statePtr->value = (ptrdiff_t)1<<tableLog;
    statePtr->stateTable = u16ptr+2;
    statePtr->symbolTT = ct + 1 + (tableLog ? (1<<(tableLog-1)) : 1);
    statePtr->stateLog = tableLog;
}


/*! FSEn_initCState2() :
*   Same as FSEn_initCState(), but the first symbol to include (which will be the last to be read)
*   uses the smallest state value possible, saving the cost of this symbol */
MEMn_STATIC void FSEn_initCState2(FSEn_CState_t* statePtr, const FSEn_CTable* ct, U32 symbol)
{
    FSEn_initCState(statePtr, ct);
    {   const FSEn_symbolCompressionTransform symbolTT = ((const FSEn_symbolCompressionTransform*)(statePtr->symbolTT))[symbol];
        const U16* stateTable = (const U16*)(statePtr->stateTable);
        U32 nbBitsOut  = (U32)((symbolTT.deltaNbBits + (1<<15)) >> 16);
        statePtr->value = (nbBitsOut << 16) - symbolTT.deltaNbBits;
        statePtr->value = stateTable[(statePtr->value >> nbBitsOut) + symbolTT.deltaFindState];
    }
}

MEMn_STATIC void FSEn_encodeSymbol(BITn_CStream_t* bitC, FSEn_CState_t* statePtr, unsigned symbol)
{
    FSEn_symbolCompressionTransform const symbolTT = ((const FSEn_symbolCompressionTransform*)(statePtr->symbolTT))[symbol];
    const U16* const stateTable = (const U16*)(statePtr->stateTable);
    U32 const nbBitsOut  = (U32)((statePtr->value + symbolTT.deltaNbBits) >> 16);
    BITn_addBits(bitC, statePtr->value, nbBitsOut);
    statePtr->value = stateTable[ (statePtr->value >> nbBitsOut) + symbolTT.deltaFindState];
}

MEMn_STATIC void FSEn_flushCState(BITn_CStream_t* bitC, const FSEn_CState_t* statePtr)
{
    BITn_addBits(bitC, statePtr->value, statePtr->stateLog);
    BITn_flushBits(bitC);
}


/* FSEn_getMaxNbBits() :
 * Approximate maximum cost of a symbol, in bits.
 * Fractional get rounded up (i.e : a symbol with a normalized frequency of 3 gives the same result as a frequency of 2)
 * note 1 : assume symbolValue is valid (<= maxSymbolValue)
 * note 2 : if freq[symbolValue]==0, @return a fake cost of tableLog+1 bits */
MEMn_STATIC U32 FSEn_getMaxNbBits(const void* symbolTTPtr, U32 symbolValue)
{
    const FSEn_symbolCompressionTransform* symbolTT = (const FSEn_symbolCompressionTransform*) symbolTTPtr;
    return (symbolTT[symbolValue].deltaNbBits + ((1<<16)-1)) >> 16;
}

/* FSEn_bitCost() :
 * Approximate symbol cost, as fractional value, using fixed-point format (accuracyLog fractional bits)
 * note 1 : assume symbolValue is valid (<= maxSymbolValue)
 * note 2 : if freq[symbolValue]==0, @return a fake cost of tableLog+1 bits */
MEMn_STATIC U32 FSEn_bitCost(const void* symbolTTPtr, U32 tableLog, U32 symbolValue, U32 accuracyLog)
{
    const FSEn_symbolCompressionTransform* symbolTT = (const FSEn_symbolCompressionTransform*) symbolTTPtr;
    U32 const minNbBits = symbolTT[symbolValue].deltaNbBits >> 16;
    U32 const threshold = (minNbBits+1) << 16;
    assert(tableLog < 16);
    assert(accuracyLog < 31-tableLog);  /* ensure enough room for renormalization double shift */
    {   U32 const tableSize = 1 << tableLog;
        U32 const deltaFromThreshold = threshold - (symbolTT[symbolValue].deltaNbBits + tableSize);
        U32 const normalizedDeltaFromThreshold = (deltaFromThreshold << accuracyLog) >> tableLog;   /* linear interpolation (very approximate) */
        U32 const bitMultiplier = 1 << accuracyLog;
        assert(symbolTT[symbolValue].deltaNbBits + tableSize <= threshold);
        assert(normalizedDeltaFromThreshold <= bitMultiplier);
        return (minNbBits+1)*bitMultiplier - normalizedDeltaFromThreshold;
    }
}


/* ======    Decompression    ====== */

typedef struct {
    U16 tableLog;
    U16 fastMode;
} FSEn_DTableHeader;   /* sizeof U32 */

typedef struct
{
    unsigned short newState;
    unsigned char  symbol;
    unsigned char  nbBits;
} FSEn_decode_t;   /* size == U32 */

MEMn_STATIC void FSEn_initDState(FSEn_DState_t* DStatePtr, BITn_DStream_t* bitD, const FSEn_DTable* dt)
{
    const void* ptr = dt;
    const FSEn_DTableHeader* const DTableH = (const FSEn_DTableHeader*)ptr;
    DStatePtr->state = BITn_readBits(bitD, DTableH->tableLog);
    BITn_reloadDStream(bitD);
    DStatePtr->table = dt + 1;
}

MEMn_STATIC BYTE FSEn_peekSymbol(const FSEn_DState_t* DStatePtr)
{
    FSEn_decode_t const DInfo = ((const FSEn_decode_t*)(DStatePtr->table))[DStatePtr->state];
    return DInfo.symbol;
}

MEMn_STATIC void FSEn_updateState(FSEn_DState_t* DStatePtr, BITn_DStream_t* bitD)
{
    FSEn_decode_t const DInfo = ((const FSEn_decode_t*)(DStatePtr->table))[DStatePtr->state];
    U32 const nbBits = DInfo.nbBits;
    size_t const lowBits = BITn_readBits(bitD, nbBits);
    DStatePtr->state = DInfo.newState + lowBits;
}

MEMn_STATIC BYTE FSEn_decodeSymbol(FSEn_DState_t* DStatePtr, BITn_DStream_t* bitD)
{
    FSEn_decode_t const DInfo = ((const FSEn_decode_t*)(DStatePtr->table))[DStatePtr->state];
    U32 const nbBits = DInfo.nbBits;
    BYTE const symbol = DInfo.symbol;
    size_t const lowBits = BITn_readBits(bitD, nbBits);

    DStatePtr->state = DInfo.newState + lowBits;
    return symbol;
}

/*! FSEn_decodeSymbolFast() :
    unsafe, only works if no symbol has a probability > 50% */
MEMn_STATIC BYTE FSEn_decodeSymbolFast(FSEn_DState_t* DStatePtr, BITn_DStream_t* bitD)
{
    FSEn_decode_t const DInfo = ((const FSEn_decode_t*)(DStatePtr->table))[DStatePtr->state];
    U32 const nbBits = DInfo.nbBits;
    BYTE const symbol = DInfo.symbol;
    size_t const lowBits = BITn_readBitsFast(bitD, nbBits);

    DStatePtr->state = DInfo.newState + lowBits;
    return symbol;
}

MEMn_STATIC unsigned FSEn_endOfDState(const FSEn_DState_t* DStatePtr)
{
    return DStatePtr->state == 0;
}



#ifndef FSEn_COMMONDEFS_ONLY

/* **************************************************************
*  Tuning parameters
****************************************************************/
/*!MEMORY_USAGE :
*  Memory usage formula : N->2^N Bytes (examples : 10 -> 1KB; 12 -> 4KB ; 16 -> 64KB; 20 -> 1MB; etc.)
*  Increasing memory usage improves compression ratio
*  Reduced memory usage can improve speed, due to cache effect
*  Recommended max value is 14, for 16KB, which nicely fits into Intel x86 L1 cache */
#ifndef FSEn_MAX_MEMORY_USAGE
#  define FSEn_MAX_MEMORY_USAGE 14
#endif
#ifndef FSEn_DEFAULT_MEMORY_USAGE
#  define FSEn_DEFAULT_MEMORY_USAGE 13
#endif
#if (FSEn_DEFAULT_MEMORY_USAGE > FSEn_MAX_MEMORY_USAGE)
#  error "FSEn_DEFAULT_MEMORY_USAGE must be <= FSEn_MAX_MEMORY_USAGE"
#endif

/*!FSEn_MAX_SYMBOL_VALUE :
*  Maximum symbol value authorized.
*  Required for proper stack allocation */
#ifndef FSEn_MAX_SYMBOL_VALUE
#  define FSEn_MAX_SYMBOL_VALUE 255
#endif

/* **************************************************************
*  template functions type & suffix
****************************************************************/
#define FSEn_FUNCTION_TYPE BYTE
#define FSEn_FUNCTION_EXTENSION
#define FSEn_DECODE_TYPE FSEn_decode_t


#endif   /* !FSEn_COMMONDEFS_ONLY */


/* ***************************************************************
*  Constants
*****************************************************************/
#define FSEn_MAX_TABLELOG  (FSEn_MAX_MEMORY_USAGE-2)
#define FSEn_MAX_TABLESIZE (1U<<FSEn_MAX_TABLELOG)
#define FSEn_MAXTABLESIZE_MASK (FSEn_MAX_TABLESIZE-1)
#define FSEn_DEFAULT_TABLELOG (FSEn_DEFAULT_MEMORY_USAGE-2)
#define FSEn_MIN_TABLELOG 5

#define FSEn_TABLELOG_ABSOLUTE_MAX 15
#if FSEn_MAX_TABLELOG > FSEn_TABLELOG_ABSOLUTE_MAX
#  error "FSEn_MAX_TABLELOG > FSEn_TABLELOG_ABSOLUTE_MAX is not supported"
#endif

#define FSEn_TABLESTEP(tableSize) (((tableSize)>>1) + ((tableSize)>>3) + 3)


#endif /* FSEn_STATIC_LINKING_ONLY */


