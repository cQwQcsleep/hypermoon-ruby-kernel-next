/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTDn_OPT_H
#define ZSTDn_OPT_H


#include "zstd_compress_internal.h"

/* used in ZSTDn_loadDictionaryContent() */
void ZSTDn_updateTree(ZSTDn_matchState_t* ms, const BYTE* ip, const BYTE* iend);

size_t ZSTDn_compressBlock_btopt(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_btultra(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_btultra2(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);


size_t ZSTDn_compressBlock_btopt_dictMatchState(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_btultra_dictMatchState(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);

size_t ZSTDn_compressBlock_btopt_extDict(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_btultra_extDict(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);

        /* note : no btultra2 variant for extDict nor dictMatchState,
         * because btultra2 is not meant to work with dictionaries
         * and is only specific for the first block (no prefix) */


#endif /* ZSTDn_OPT_H */
