/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTDn_LDM_H
#define ZSTDn_LDM_H


#include "zstd_compress_internal.h"   /* ldmParams_t, U32 */
#include "include/zstd.h"   /* ZSTDn_CCtx, size_t */

/*-*************************************
*  Long distance matching
***************************************/

#define ZSTDn_LDM_DEFAULT_WINDOW_LOG ZSTDn_WINDOWLOG_LIMIT_DEFAULT

void ZSTDn_ldm_fillHashTable(
            ldmState_t* state, const BYTE* ip,
            const BYTE* iend, ldmParams_t const* params);

/*
 * ZSTDn_ldm_generateSequences():
 *
 * Generates the sequences using the long distance match finder.
 * Generates long range matching sequences in `sequences`, which parse a prefix
 * of the source. `sequences` must be large enough to store every sequence,
 * which can be checked with `ZSTDn_ldm_getMaxNbSeq()`.
 * @returns 0 or an error code.
 *
 * NOTE: The user must have called ZSTDn_window_update() for all of the input
 * they have, even if they pass it to ZSTDn_ldm_generateSequences() in chunks.
 * NOTE: This function returns an error if it runs out of space to store
 *       sequences.
 */
size_t ZSTDn_ldm_generateSequences(
            ldmState_t* ldms, rawSeqStore_t* sequences,
            ldmParams_t const* params, void const* src, size_t srcSize);

/*
 * ZSTDn_ldm_blockCompress():
 *
 * Compresses a block using the predefined sequences, along with a secondary
 * block compressor. The literals section of every sequence is passed to the
 * secondary block compressor, and those sequences are interspersed with the
 * predefined sequences. Returns the length of the last literals.
 * Updates `rawSeqStore.pos` to indicate how many sequences have been consumed.
 * `rawSeqStore.seq` may also be updated to split the last sequence between two
 * blocks.
 * @return The length of the last literals.
 *
 * NOTE: The source must be at most the maximum block size, but the predefined
 * sequences can be any size, and may be longer than the block. In the case that
 * they are longer than the block, the last sequences may need to be split into
 * two. We handle that case correctly, and update `rawSeqStore` appropriately.
 * NOTE: This function does not return any errors.
 */
size_t ZSTDn_ldm_blockCompress(rawSeqStore_t* rawSeqStore,
            ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
            void const* src, size_t srcSize);

/*
 * ZSTDn_ldm_skipSequences():
 *
 * Skip past `srcSize` bytes worth of sequences in `rawSeqStore`.
 * Avoids emitting matches less than `minMatch` bytes.
 * Must be called for data that is not passed to ZSTDn_ldm_blockCompress().
 */
void ZSTDn_ldm_skipSequences(rawSeqStore_t* rawSeqStore, size_t srcSize,
    U32 const minMatch);

/* ZSTDn_ldm_skipRawSeqStoreBytes():
 * Moves forward in rawSeqStore by nbBytes, updating fields 'pos' and 'posInSequence'.
 * Not to be used in conjunction with ZSTDn_ldm_skipSequences().
 * Must be called for data with is not passed to ZSTDn_ldm_blockCompress().
 */
void ZSTDn_ldm_skipRawSeqStoreBytes(rawSeqStore_t* rawSeqStore, size_t nbBytes);

/* ZSTDn_ldm_getTableSize() :
 *  Estimate the space needed for long distance matching tables or 0 if LDM is
 *  disabled.
 */
size_t ZSTDn_ldm_getTableSize(ldmParams_t params);

/* ZSTDn_ldm_getSeqSpace() :
 *  Return an upper bound on the number of sequences that can be produced by
 *  the long distance matcher, or 0 if LDM is disabled.
 */
size_t ZSTDn_ldm_getMaxNbSeq(ldmParams_t params, size_t maxChunkSize);

/* ZSTDn_ldm_adjustParameters() :
 *  If the params->hashRateLog is not set, set it to its default value based on
 *  windowLog and params->hashLog.
 *
 *  Ensures that params->bucketSizeLog is <= params->hashLog (setting it to
 *  params->hashLog if it is not).
 *
 *  Ensures that the minMatchLength >= targetLength during optimal parsing.
 */
void ZSTDn_ldm_adjustParameters(ldmParams_t* params,
                               ZSTDn_compressionParameters const* cParams);


#endif /* ZSTDn_FAST_H */
