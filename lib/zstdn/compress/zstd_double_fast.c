/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "zstd_compress_internal.h"
#include "zstd_double_fast.h"


void ZSTDn_fillDoubleHashTable(ZSTDn_matchState_t* ms,
                              void const* end, ZSTDn_dictTableLoadMethod_e dtlm)
{
    const ZSTDn_compressionParameters* const cParams = &ms->cParams;
    U32* const hashLarge = ms->hashTable;
    U32  const hBitsL = cParams->hashLog;
    U32  const mls = cParams->minMatch;
    U32* const hashSmall = ms->chainTable;
    U32  const hBitsS = cParams->chainLog;
    const BYTE* const base = ms->window.base;
    const BYTE* ip = base + ms->nextToUpdate;
    const BYTE* const iend = ((const BYTE*)end) - HASH_READ_SIZE;
    const U32 fastHashFillStep = 3;

    /* Always insert every fastHashFillStep position into the hash tables.
     * Insert the other positions into the large hash table if their entry
     * is empty.
     */
    for (; ip + fastHashFillStep - 1 <= iend; ip += fastHashFillStep) {
        U32 const curr = (U32)(ip - base);
        U32 i;
        for (i = 0; i < fastHashFillStep; ++i) {
            size_t const smHash = ZSTDn_hashPtr(ip + i, hBitsS, mls);
            size_t const lgHash = ZSTDn_hashPtr(ip + i, hBitsL, 8);
            if (i == 0)
                hashSmall[smHash] = curr + i;
            if (i == 0 || hashLarge[lgHash] == 0)
                hashLarge[lgHash] = curr + i;
            /* Only load extra positions for ZSTDn_dtlm_full */
            if (dtlm == ZSTDn_dtlm_fast)
                break;
    }   }
}


FORCE_INLINE_TEMPLATE
size_t ZSTDn_compressBlock_doubleFast_generic(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize,
        U32 const mls /* template */, ZSTDn_dictMode_e const dictMode)
{
    ZSTDn_compressionParameters const* cParams = &ms->cParams;
    U32* const hashLong = ms->hashTable;
    const U32 hBitsL = cParams->hashLog;
    U32* const hashSmall = ms->chainTable;
    const U32 hBitsS = cParams->chainLog;
    const BYTE* const base = ms->window.base;
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const U32 endIndex = (U32)((size_t)(istart - base) + srcSize);
    /* presumes that, if there is a dictionary, it must be using Attach mode */
    const U32 prefixLowestIndex = ZSTDn_getLowestPrefixIndex(ms, endIndex, cParams->windowLog);
    const BYTE* const prefixLowest = base + prefixLowestIndex;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - HASH_READ_SIZE;
    U32 offset_1=rep[0], offset_2=rep[1];
    U32 offsetSaved = 0;

    const ZSTDn_matchState_t* const dms = ms->dictMatchState;
    const ZSTDn_compressionParameters* const dictCParams =
                                     dictMode == ZSTDn_dictMatchState ?
                                     &dms->cParams : NULL;
    const U32* const dictHashLong  = dictMode == ZSTDn_dictMatchState ?
                                     dms->hashTable : NULL;
    const U32* const dictHashSmall = dictMode == ZSTDn_dictMatchState ?
                                     dms->chainTable : NULL;
    const U32 dictStartIndex       = dictMode == ZSTDn_dictMatchState ?
                                     dms->window.dictLimit : 0;
    const BYTE* const dictBase     = dictMode == ZSTDn_dictMatchState ?
                                     dms->window.base : NULL;
    const BYTE* const dictStart    = dictMode == ZSTDn_dictMatchState ?
                                     dictBase + dictStartIndex : NULL;
    const BYTE* const dictEnd      = dictMode == ZSTDn_dictMatchState ?
                                     dms->window.nextSrc : NULL;
    const U32 dictIndexDelta       = dictMode == ZSTDn_dictMatchState ?
                                     prefixLowestIndex - (U32)(dictEnd - dictBase) :
                                     0;
    const U32 dictHBitsL           = dictMode == ZSTDn_dictMatchState ?
                                     dictCParams->hashLog : hBitsL;
    const U32 dictHBitsS           = dictMode == ZSTDn_dictMatchState ?
                                     dictCParams->chainLog : hBitsS;
    const U32 dictAndPrefixLength  = (U32)((ip - prefixLowest) + (dictEnd - dictStart));

    DEBUGLOG(5, "ZSTDn_compressBlock_doubleFast_generic");

    assert(dictMode == ZSTDn_noDict || dictMode == ZSTDn_dictMatchState);

    /* if a dictionary is attached, it must be within window range */
    if (dictMode == ZSTDn_dictMatchState) {
        assert(ms->window.dictLimit + (1U << cParams->windowLog) >= endIndex);
    }

    /* init */
    ip += (dictAndPrefixLength == 0);
    if (dictMode == ZSTDn_noDict) {
        U32 const curr = (U32)(ip - base);
        U32 const windowLow = ZSTDn_getLowestPrefixIndex(ms, curr, cParams->windowLog);
        U32 const maxRep = curr - windowLow;
        if (offset_2 > maxRep) offsetSaved = offset_2, offset_2 = 0;
        if (offset_1 > maxRep) offsetSaved = offset_1, offset_1 = 0;
    }
    if (dictMode == ZSTDn_dictMatchState) {
        /* dictMatchState repCode checks don't currently handle repCode == 0
         * disabling. */
        assert(offset_1 <= dictAndPrefixLength);
        assert(offset_2 <= dictAndPrefixLength);
    }

    /* Main Search Loop */
    while (ip < ilimit) {   /* < instead of <=, because repcode check at (ip+1) */
        size_t mLength;
        U32 offset;
        size_t const h2 = ZSTDn_hashPtr(ip, hBitsL, 8);
        size_t const h = ZSTDn_hashPtr(ip, hBitsS, mls);
        size_t const dictHL = ZSTDn_hashPtr(ip, dictHBitsL, 8);
        size_t const dictHS = ZSTDn_hashPtr(ip, dictHBitsS, mls);
        U32 const curr = (U32)(ip-base);
        U32 const matchIndexL = hashLong[h2];
        U32 matchIndexS = hashSmall[h];
        const BYTE* matchLong = base + matchIndexL;
        const BYTE* match = base + matchIndexS;
        const U32 repIndex = curr + 1 - offset_1;
        const BYTE* repMatch = (dictMode == ZSTDn_dictMatchState
                            && repIndex < prefixLowestIndex) ?
                               dictBase + (repIndex - dictIndexDelta) :
                               base + repIndex;
        hashLong[h2] = hashSmall[h] = curr;   /* update hash tables */

        /* check dictMatchState repcode */
        if (dictMode == ZSTDn_dictMatchState
            && ((U32)((prefixLowestIndex-1) - repIndex) >= 3 /* intentional underflow */)
            && (MEMn_read32(repMatch) == MEMn_read32(ip+1)) ) {
            const BYTE* repMatchEnd = repIndex < prefixLowestIndex ? dictEnd : iend;
            mLength = ZSTDn_count_2segments(ip+1+4, repMatch+4, iend, repMatchEnd, prefixLowest) + 4;
            ip++;
            ZSTDn_storeSeq(seqStore, (size_t)(ip-anchor), anchor, iend, 0, mLength-MINMATCH);
            goto _match_stored;
        }

        /* check noDict repcode */
        if ( dictMode == ZSTDn_noDict
          && ((offset_1 > 0) & (MEMn_read32(ip+1-offset_1) == MEMn_read32(ip+1)))) {
            mLength = ZSTDn_count(ip+1+4, ip+1+4-offset_1, iend) + 4;
            ip++;
            ZSTDn_storeSeq(seqStore, (size_t)(ip-anchor), anchor, iend, 0, mLength-MINMATCH);
            goto _match_stored;
        }

        if (matchIndexL > prefixLowestIndex) {
            /* check prefix long match */
            if (MEMn_read64(matchLong) == MEMn_read64(ip)) {
                mLength = ZSTDn_count(ip+8, matchLong+8, iend) + 8;
                offset = (U32)(ip-matchLong);
                while (((ip>anchor) & (matchLong>prefixLowest)) && (ip[-1] == matchLong[-1])) { ip--; matchLong--; mLength++; } /* catch up */
                goto _match_found;
            }
        } else if (dictMode == ZSTDn_dictMatchState) {
            /* check dictMatchState long match */
            U32 const dictMatchIndexL = dictHashLong[dictHL];
            const BYTE* dictMatchL = dictBase + dictMatchIndexL;
            assert(dictMatchL < dictEnd);

            if (dictMatchL > dictStart && MEMn_read64(dictMatchL) == MEMn_read64(ip)) {
                mLength = ZSTDn_count_2segments(ip+8, dictMatchL+8, iend, dictEnd, prefixLowest) + 8;
                offset = (U32)(curr - dictMatchIndexL - dictIndexDelta);
                while (((ip>anchor) & (dictMatchL>dictStart)) && (ip[-1] == dictMatchL[-1])) { ip--; dictMatchL--; mLength++; } /* catch up */
                goto _match_found;
        }   }

        if (matchIndexS > prefixLowestIndex) {
            /* check prefix short match */
            if (MEMn_read32(match) == MEMn_read32(ip)) {
                goto _search_next_long;
            }
        } else if (dictMode == ZSTDn_dictMatchState) {
            /* check dictMatchState short match */
            U32 const dictMatchIndexS = dictHashSmall[dictHS];
            match = dictBase + dictMatchIndexS;
            matchIndexS = dictMatchIndexS + dictIndexDelta;

            if (match > dictStart && MEMn_read32(match) == MEMn_read32(ip)) {
                goto _search_next_long;
        }   }

        ip += ((ip-anchor) >> kSearchStrength) + 1;
#if defined(__aarch64__)
        PREFETCH_L1(ip+256);
#endif
        continue;

_search_next_long:

        {   size_t const hl3 = ZSTDn_hashPtr(ip+1, hBitsL, 8);
            size_t const dictHLNext = ZSTDn_hashPtr(ip+1, dictHBitsL, 8);
            U32 const matchIndexL3 = hashLong[hl3];
            const BYTE* matchL3 = base + matchIndexL3;
            hashLong[hl3] = curr + 1;

            /* check prefix long +1 match */
            if (matchIndexL3 > prefixLowestIndex) {
                if (MEMn_read64(matchL3) == MEMn_read64(ip+1)) {
                    mLength = ZSTDn_count(ip+9, matchL3+8, iend) + 8;
                    ip++;
                    offset = (U32)(ip-matchL3);
                    while (((ip>anchor) & (matchL3>prefixLowest)) && (ip[-1] == matchL3[-1])) { ip--; matchL3--; mLength++; } /* catch up */
                    goto _match_found;
                }
            } else if (dictMode == ZSTDn_dictMatchState) {
                /* check dict long +1 match */
                U32 const dictMatchIndexL3 = dictHashLong[dictHLNext];
                const BYTE* dictMatchL3 = dictBase + dictMatchIndexL3;
                assert(dictMatchL3 < dictEnd);
                if (dictMatchL3 > dictStart && MEMn_read64(dictMatchL3) == MEMn_read64(ip+1)) {
                    mLength = ZSTDn_count_2segments(ip+1+8, dictMatchL3+8, iend, dictEnd, prefixLowest) + 8;
                    ip++;
                    offset = (U32)(curr + 1 - dictMatchIndexL3 - dictIndexDelta);
                    while (((ip>anchor) & (dictMatchL3>dictStart)) && (ip[-1] == dictMatchL3[-1])) { ip--; dictMatchL3--; mLength++; } /* catch up */
                    goto _match_found;
        }   }   }

        /* if no long +1 match, explore the short match we found */
        if (dictMode == ZSTDn_dictMatchState && matchIndexS < prefixLowestIndex) {
            mLength = ZSTDn_count_2segments(ip+4, match+4, iend, dictEnd, prefixLowest) + 4;
            offset = (U32)(curr - matchIndexS);
            while (((ip>anchor) & (match>dictStart)) && (ip[-1] == match[-1])) { ip--; match--; mLength++; } /* catch up */
        } else {
            mLength = ZSTDn_count(ip+4, match+4, iend) + 4;
            offset = (U32)(ip - match);
            while (((ip>anchor) & (match>prefixLowest)) && (ip[-1] == match[-1])) { ip--; match--; mLength++; } /* catch up */
        }

_match_found:
        offset_2 = offset_1;
        offset_1 = offset;

        ZSTDn_storeSeq(seqStore, (size_t)(ip-anchor), anchor, iend, offset + ZSTDn_REP_MOVE, mLength-MINMATCH);

_match_stored:
        /* match found */
        ip += mLength;
        anchor = ip;

        if (ip <= ilimit) {
            /* Complementary insertion */
            /* done after iLimit test, as candidates could be > iend-8 */
            {   U32 const indexToInsert = curr+2;
                hashLong[ZSTDn_hashPtr(base+indexToInsert, hBitsL, 8)] = indexToInsert;
                hashLong[ZSTDn_hashPtr(ip-2, hBitsL, 8)] = (U32)(ip-2-base);
                hashSmall[ZSTDn_hashPtr(base+indexToInsert, hBitsS, mls)] = indexToInsert;
                hashSmall[ZSTDn_hashPtr(ip-1, hBitsS, mls)] = (U32)(ip-1-base);
            }

            /* check immediate repcode */
            if (dictMode == ZSTDn_dictMatchState) {
                while (ip <= ilimit) {
                    U32 const current2 = (U32)(ip-base);
                    U32 const repIndex2 = current2 - offset_2;
                    const BYTE* repMatch2 = dictMode == ZSTDn_dictMatchState
                        && repIndex2 < prefixLowestIndex ?
                            dictBase + repIndex2 - dictIndexDelta :
                            base + repIndex2;
                    if ( ((U32)((prefixLowestIndex-1) - (U32)repIndex2) >= 3 /* intentional overflow */)
                       && (MEMn_read32(repMatch2) == MEMn_read32(ip)) ) {
                        const BYTE* const repEnd2 = repIndex2 < prefixLowestIndex ? dictEnd : iend;
                        size_t const repLength2 = ZSTDn_count_2segments(ip+4, repMatch2+4, iend, repEnd2, prefixLowest) + 4;
                        U32 tmpOffset = offset_2; offset_2 = offset_1; offset_1 = tmpOffset;   /* swap offset_2 <=> offset_1 */
                        ZSTDn_storeSeq(seqStore, 0, anchor, iend, 0, repLength2-MINMATCH);
                        hashSmall[ZSTDn_hashPtr(ip, hBitsS, mls)] = current2;
                        hashLong[ZSTDn_hashPtr(ip, hBitsL, 8)] = current2;
                        ip += repLength2;
                        anchor = ip;
                        continue;
                    }
                    break;
            }   }

            if (dictMode == ZSTDn_noDict) {
                while ( (ip <= ilimit)
                     && ( (offset_2>0)
                        & (MEMn_read32(ip) == MEMn_read32(ip - offset_2)) )) {
                    /* store sequence */
                    size_t const rLength = ZSTDn_count(ip+4, ip+4-offset_2, iend) + 4;
                    U32 const tmpOff = offset_2; offset_2 = offset_1; offset_1 = tmpOff;  /* swap offset_2 <=> offset_1 */
                    hashSmall[ZSTDn_hashPtr(ip, hBitsS, mls)] = (U32)(ip-base);
                    hashLong[ZSTDn_hashPtr(ip, hBitsL, 8)] = (U32)(ip-base);
                    ZSTDn_storeSeq(seqStore, 0, anchor, iend, 0, rLength-MINMATCH);
                    ip += rLength;
                    anchor = ip;
                    continue;   /* faster when present ... (?) */
        }   }   }
    }   /* while (ip < ilimit) */

    /* save reps for next block */
    rep[0] = offset_1 ? offset_1 : offsetSaved;
    rep[1] = offset_2 ? offset_2 : offsetSaved;

    /* Return the last literals size */
    return (size_t)(iend - anchor);
}


size_t ZSTDn_compressBlock_doubleFast(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize)
{
    const U32 mls = ms->cParams.minMatch;
    switch(mls)
    {
    default: /* includes case 3 */
    case 4 :
        return ZSTDn_compressBlock_doubleFast_generic(ms, seqStore, rep, src, srcSize, 4, ZSTDn_noDict);
    case 5 :
        return ZSTDn_compressBlock_doubleFast_generic(ms, seqStore, rep, src, srcSize, 5, ZSTDn_noDict);
    case 6 :
        return ZSTDn_compressBlock_doubleFast_generic(ms, seqStore, rep, src, srcSize, 6, ZSTDn_noDict);
    case 7 :
        return ZSTDn_compressBlock_doubleFast_generic(ms, seqStore, rep, src, srcSize, 7, ZSTDn_noDict);
    }
}


size_t ZSTDn_compressBlock_doubleFast_dictMatchState(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize)
{
    const U32 mls = ms->cParams.minMatch;
    switch(mls)
    {
    default: /* includes case 3 */
    case 4 :
        return ZSTDn_compressBlock_doubleFast_generic(ms, seqStore, rep, src, srcSize, 4, ZSTDn_dictMatchState);
    case 5 :
        return ZSTDn_compressBlock_doubleFast_generic(ms, seqStore, rep, src, srcSize, 5, ZSTDn_dictMatchState);
    case 6 :
        return ZSTDn_compressBlock_doubleFast_generic(ms, seqStore, rep, src, srcSize, 6, ZSTDn_dictMatchState);
    case 7 :
        return ZSTDn_compressBlock_doubleFast_generic(ms, seqStore, rep, src, srcSize, 7, ZSTDn_dictMatchState);
    }
}


static size_t ZSTDn_compressBlock_doubleFast_extDict_generic(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize,
        U32 const mls /* template */)
{
    ZSTDn_compressionParameters const* cParams = &ms->cParams;
    U32* const hashLong = ms->hashTable;
    U32  const hBitsL = cParams->hashLog;
    U32* const hashSmall = ms->chainTable;
    U32  const hBitsS = cParams->chainLog;
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;
    const BYTE* const base = ms->window.base;
    const U32   endIndex = (U32)((size_t)(istart - base) + srcSize);
    const U32   lowLimit = ZSTDn_getLowestMatchIndex(ms, endIndex, cParams->windowLog);
    const U32   dictStartIndex = lowLimit;
    const U32   dictLimit = ms->window.dictLimit;
    const U32   prefixStartIndex = (dictLimit > lowLimit) ? dictLimit : lowLimit;
    const BYTE* const prefixStart = base + prefixStartIndex;
    const BYTE* const dictBase = ms->window.dictBase;
    const BYTE* const dictStart = dictBase + dictStartIndex;
    const BYTE* const dictEnd = dictBase + prefixStartIndex;
    U32 offset_1=rep[0], offset_2=rep[1];

    DEBUGLOG(5, "ZSTDn_compressBlock_doubleFast_extDict_generic (srcSize=%zu)", srcSize);

    /* if extDict is invalidated due to maxDistance, switch to "regular" variant */
    if (prefixStartIndex == dictStartIndex)
        return ZSTDn_compressBlock_doubleFast_generic(ms, seqStore, rep, src, srcSize, mls, ZSTDn_noDict);

    /* Search Loop */
    while (ip < ilimit) {  /* < instead of <=, because (ip+1) */
        const size_t hSmall = ZSTDn_hashPtr(ip, hBitsS, mls);
        const U32 matchIndex = hashSmall[hSmall];
        const BYTE* const matchBase = matchIndex < prefixStartIndex ? dictBase : base;
        const BYTE* match = matchBase + matchIndex;

        const size_t hLong = ZSTDn_hashPtr(ip, hBitsL, 8);
        const U32 matchLongIndex = hashLong[hLong];
        const BYTE* const matchLongBase = matchLongIndex < prefixStartIndex ? dictBase : base;
        const BYTE* matchLong = matchLongBase + matchLongIndex;

        const U32 curr = (U32)(ip-base);
        const U32 repIndex = curr + 1 - offset_1;   /* offset_1 expected <= curr +1 */
        const BYTE* const repBase = repIndex < prefixStartIndex ? dictBase : base;
        const BYTE* const repMatch = repBase + repIndex;
        size_t mLength;
        hashSmall[hSmall] = hashLong[hLong] = curr;   /* update hash table */

        if ((((U32)((prefixStartIndex-1) - repIndex) >= 3) /* intentional underflow : ensure repIndex doesn't overlap dict + prefix */
            & (repIndex > dictStartIndex))
          && (MEMn_read32(repMatch) == MEMn_read32(ip+1)) ) {
            const BYTE* repMatchEnd = repIndex < prefixStartIndex ? dictEnd : iend;
            mLength = ZSTDn_count_2segments(ip+1+4, repMatch+4, iend, repMatchEnd, prefixStart) + 4;
            ip++;
            ZSTDn_storeSeq(seqStore, (size_t)(ip-anchor), anchor, iend, 0, mLength-MINMATCH);
        } else {
            if ((matchLongIndex > dictStartIndex) && (MEMn_read64(matchLong) == MEMn_read64(ip))) {
                const BYTE* const matchEnd = matchLongIndex < prefixStartIndex ? dictEnd : iend;
                const BYTE* const lowMatchPtr = matchLongIndex < prefixStartIndex ? dictStart : prefixStart;
                U32 offset;
                mLength = ZSTDn_count_2segments(ip+8, matchLong+8, iend, matchEnd, prefixStart) + 8;
                offset = curr - matchLongIndex;
                while (((ip>anchor) & (matchLong>lowMatchPtr)) && (ip[-1] == matchLong[-1])) { ip--; matchLong--; mLength++; }   /* catch up */
                offset_2 = offset_1;
                offset_1 = offset;
                ZSTDn_storeSeq(seqStore, (size_t)(ip-anchor), anchor, iend, offset + ZSTDn_REP_MOVE, mLength-MINMATCH);

            } else if ((matchIndex > dictStartIndex) && (MEMn_read32(match) == MEMn_read32(ip))) {
                size_t const h3 = ZSTDn_hashPtr(ip+1, hBitsL, 8);
                U32 const matchIndex3 = hashLong[h3];
                const BYTE* const match3Base = matchIndex3 < prefixStartIndex ? dictBase : base;
                const BYTE* match3 = match3Base + matchIndex3;
                U32 offset;
                hashLong[h3] = curr + 1;
                if ( (matchIndex3 > dictStartIndex) && (MEMn_read64(match3) == MEMn_read64(ip+1)) ) {
                    const BYTE* const matchEnd = matchIndex3 < prefixStartIndex ? dictEnd : iend;
                    const BYTE* const lowMatchPtr = matchIndex3 < prefixStartIndex ? dictStart : prefixStart;
                    mLength = ZSTDn_count_2segments(ip+9, match3+8, iend, matchEnd, prefixStart) + 8;
                    ip++;
                    offset = curr+1 - matchIndex3;
                    while (((ip>anchor) & (match3>lowMatchPtr)) && (ip[-1] == match3[-1])) { ip--; match3--; mLength++; } /* catch up */
                } else {
                    const BYTE* const matchEnd = matchIndex < prefixStartIndex ? dictEnd : iend;
                    const BYTE* const lowMatchPtr = matchIndex < prefixStartIndex ? dictStart : prefixStart;
                    mLength = ZSTDn_count_2segments(ip+4, match+4, iend, matchEnd, prefixStart) + 4;
                    offset = curr - matchIndex;
                    while (((ip>anchor) & (match>lowMatchPtr)) && (ip[-1] == match[-1])) { ip--; match--; mLength++; }   /* catch up */
                }
                offset_2 = offset_1;
                offset_1 = offset;
                ZSTDn_storeSeq(seqStore, (size_t)(ip-anchor), anchor, iend, offset + ZSTDn_REP_MOVE, mLength-MINMATCH);

            } else {
                ip += ((ip-anchor) >> kSearchStrength) + 1;
                continue;
        }   }

        /* move to next sequence start */
        ip += mLength;
        anchor = ip;

        if (ip <= ilimit) {
            /* Complementary insertion */
            /* done after iLimit test, as candidates could be > iend-8 */
            {   U32 const indexToInsert = curr+2;
                hashLong[ZSTDn_hashPtr(base+indexToInsert, hBitsL, 8)] = indexToInsert;
                hashLong[ZSTDn_hashPtr(ip-2, hBitsL, 8)] = (U32)(ip-2-base);
                hashSmall[ZSTDn_hashPtr(base+indexToInsert, hBitsS, mls)] = indexToInsert;
                hashSmall[ZSTDn_hashPtr(ip-1, hBitsS, mls)] = (U32)(ip-1-base);
            }

            /* check immediate repcode */
            while (ip <= ilimit) {
                U32 const current2 = (U32)(ip-base);
                U32 const repIndex2 = current2 - offset_2;
                const BYTE* repMatch2 = repIndex2 < prefixStartIndex ? dictBase + repIndex2 : base + repIndex2;
                if ( (((U32)((prefixStartIndex-1) - repIndex2) >= 3)   /* intentional overflow : ensure repIndex2 doesn't overlap dict + prefix */
                    & (repIndex2 > dictStartIndex))
                  && (MEMn_read32(repMatch2) == MEMn_read32(ip)) ) {
                    const BYTE* const repEnd2 = repIndex2 < prefixStartIndex ? dictEnd : iend;
                    size_t const repLength2 = ZSTDn_count_2segments(ip+4, repMatch2+4, iend, repEnd2, prefixStart) + 4;
                    U32 const tmpOffset = offset_2; offset_2 = offset_1; offset_1 = tmpOffset;   /* swap offset_2 <=> offset_1 */
                    ZSTDn_storeSeq(seqStore, 0, anchor, iend, 0, repLength2-MINMATCH);
                    hashSmall[ZSTDn_hashPtr(ip, hBitsS, mls)] = current2;
                    hashLong[ZSTDn_hashPtr(ip, hBitsL, 8)] = current2;
                    ip += repLength2;
                    anchor = ip;
                    continue;
                }
                break;
    }   }   }

    /* save reps for next block */
    rep[0] = offset_1;
    rep[1] = offset_2;

    /* Return the last literals size */
    return (size_t)(iend - anchor);
}


size_t ZSTDn_compressBlock_doubleFast_extDict(
        ZSTDn_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTDn_REP_NUM],
        void const* src, size_t srcSize)
{
    U32 const mls = ms->cParams.minMatch;
    switch(mls)
    {
    default: /* includes case 3 */
    case 4 :
        return ZSTDn_compressBlock_doubleFast_extDict_generic(ms, seqStore, rep, src, srcSize, 4);
    case 5 :
        return ZSTDn_compressBlock_doubleFast_extDict_generic(ms, seqStore, rep, src, srcSize, 5);
    case 6 :
        return ZSTDn_compressBlock_doubleFast_extDict_generic(ms, seqStore, rep, src, srcSize, 6);
    case 7 :
        return ZSTDn_compressBlock_doubleFast_extDict_generic(ms, seqStore, rep, src, srcSize, 7);
    }
}
