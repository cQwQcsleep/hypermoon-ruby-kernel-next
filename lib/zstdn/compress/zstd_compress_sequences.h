/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTDn_COMPRESS_SEQUENCES_H
#define ZSTDn_COMPRESS_SEQUENCES_H

#include "../common/fse.h" /* FSEn_repeat, FSEn_CTable */
#include "../common/zstd_internal.h" /* symbolEncodingType_e, ZSTDn_strategy */

typedef enum {
    ZSTDn_defaultDisallowed = 0,
    ZSTDn_defaultAllowed = 1
} ZSTDn_defaultPolicy_e;

symbolEncodingType_e
ZSTDn_selectEncodingType(
        FSEn_repeat* repeatMode, unsigned const* count, unsigned const max,
        size_t const mostFrequent, size_t nbSeq, unsigned const FSELog,
        FSEn_CTable const* prevCTable,
        short const* defaultNorm, U32 defaultNormLog,
        ZSTDn_defaultPolicy_e const isDefaultAllowed,
        ZSTDn_strategy const strategy);

size_t
ZSTDn_buildCTable(void* dst, size_t dstCapacity,
                FSEn_CTable* nextCTable, U32 FSELog, symbolEncodingType_e type,
                unsigned* count, U32 max,
                const BYTE* codeTable, size_t nbSeq,
                const S16* defaultNorm, U32 defaultNormLog, U32 defaultMax,
                const FSEn_CTable* prevCTable, size_t prevCTableSize,
                void* entropyWorkspace, size_t entropyWorkspaceSize);

size_t ZSTDn_encodeSequences(
            void* dst, size_t dstCapacity,
            FSEn_CTable const* CTable_MatchLength, BYTE const* mlCodeTable,
            FSEn_CTable const* CTable_OffsetBits, BYTE const* ofCodeTable,
            FSEn_CTable const* CTable_LitLength, BYTE const* llCodeTable,
            seqDef const* sequences, size_t nbSeq, int longOffsets, int bmi2);

size_t ZSTDn_fseBitCost(
    FSEn_CTable const* ctable,
    unsigned const* count,
    unsigned const max);

size_t ZSTDn_crossEntropyCost(short const* norm, unsigned accuracyLog,
                             unsigned const* count, unsigned const max);
#endif /* ZSTDn_COMPRESS_SEQUENCES_H */
