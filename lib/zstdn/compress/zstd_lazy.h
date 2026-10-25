/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTDn_LAZY_H
#define ZSTDn_LAZY_H


#include "zstd_compress_internal.h"

/*
 * Dedicated Dictionary Search Structure bucket log. In the
 * ZSTDn_dedicatedDictSearch mode, the hashTable has
 * 2 ** ZSTDn_LAZY_DDSS_BUCKET_LOG entries in each bucket, rather than just
 * one.
 */
#define ZSTDn_LAZY_DDSS_BUCKET_LOG 2

U32 ZSTDn_insertAndFindFirstIndex(ZSTDn_matchState_t* ms, const BYTE* ip);

void ZSTDn_dedicatedDictSearch_lazy_loadDictionary(ZSTDn_matchState_t* ms, const BYTE* const ip);

void ZSTDn_preserveUnsortedMark (U32* const table, U32 const size, U32 const reducerValue);  /*! used in ZSTDn_reduceIndex(). preemptively increase value of ZSTDn_DUBT_UNSORTED_MARK */

size_t ZSTDn_compressBlock_btlazy2(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_lazy2(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_lazy(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_greedy(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);

size_t ZSTDn_compressBlock_btlazy2_dictMatchState(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_lazy2_dictMatchState(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_lazy_dictMatchState(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_greedy_dictMatchState(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);

size_t ZSTDn_compressBlock_lazy2_dedicatedDictSearch(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_lazy_dedicatedDictSearch(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_greedy_dedicatedDictSearch(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);

size_t ZSTDn_compressBlock_greedy_extDict(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_lazy_extDict(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_lazy2_extDict(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTDn_compressBlock_btlazy2_extDict(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize);


#endif /* ZSTDn_LAZY_H */
