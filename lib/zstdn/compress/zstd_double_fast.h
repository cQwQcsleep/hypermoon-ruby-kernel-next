/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTDn_DOUBLE_FAST_H
#define ZSTDn_DOUBLE_FAST_H


#include "../common/mem.h"      /* U32 */
#include "zstd_compress_internal.h"     /* ZSTDn_CCtx, size_t */

void ZSTDn_fillDoubleHashTable(ZSTDn_matchState_t* ms,
                              void const* end, ZSTDn_dictTableLoadMethod_e dtlm);
size_t ZSTDn_compressBlock_doubleFast(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_doubleFast_dictMatchState(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_doubleFast_extDict(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);



#endif /* ZSTDn_DOUBLE_FAST_H */
