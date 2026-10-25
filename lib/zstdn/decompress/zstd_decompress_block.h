/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


#ifndef ZSTDn_DEC_BLOCK_H
#define ZSTDn_DEC_BLOCK_H

/*-*******************************************************
 *  Dependencies
 *********************************************************/
#include "../common/zstd_deps.h"   /* size_t */
#include "include/zstd.h"    /* DCtx, and some public functions */
#include "../common/zstd_internal.h"  /* blockProperties_t, and some public functions */
#include "zstd_decompress_internal.h"  /* ZSTDn_seqSymbol */


/* ===   Prototypes   === */

/* note: prototypes already published within `zstd.h` :
 * ZSTDn_decompressBlock()
 */

/* note: prototypes already published within `zstdn_internal.h` :
 * ZSTDn_getcBlockSize()
 * ZSTDn_decodeSeqHeaders()
 */


/* ZSTDn_decompressBlock_internal() :
 * decompress block, starting at `src`,
 * into destination buffer `dst`.
 * @return : decompressed block size,
 *           or an error code (which can be tested using ZSTDn_isError())
 */
size_t ZSTDn_decompressBlock_internal(ZSTDn_DCtx* dctx,
                               void* dst, size_t dstCapacity,
                         const void* src, size_t srcSize, const int frame);

/* ZSTDn_buildFSETable() :
 * generate FSE decoding table for one symbol (ll, ml or off)
 * this function must be called with valid parameters only
 * (dt is large enough, normalizedCounter distribution total is a power of 2, max is within range, etc.)
 * in which case it cannot fail.
 * The workspace must be 4-byte aligned and at least ZSTDn_BUILD_FSE_TABLE_WKSP_SIZE bytes, which is
 * defined in zstdn_decompress_internal.h.
 * Internal use only.
 */
void ZSTDn_buildFSETable(ZSTDn_seqSymbol* dt,
             const short* normalizedCounter, unsigned maxSymbolValue,
             const U32* baseValue, const U32* nbAdditionalBits,
                   unsigned tableLog, void* wksp, size_t wkspSize,
                   int bmi2);


#endif /* ZSTDn_DEC_BLOCK_H */
