/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* zstdn_ddict.c :
 * concentrates all logic that needs to know the internals of ZSTDn_DDict object */

/*-*******************************************************
*  Dependencies
*********************************************************/
#include "../common/zstd_deps.h"   /* ZSTDn_memcpy, ZSTDn_memmove, ZSTDn_memset */
#include "../common/cpu.h"         /* bmi2 */
#include "../common/mem.h"         /* low level memory routines */
#define FSEn_STATIC_LINKING_ONLY
#include "../common/fse.h"
#define HUFn_STATIC_LINKING_ONLY
#include "../common/huf.h"
#include "zstd_decompress_internal.h"
#include "zstd_ddict.h"




/*-*******************************************************
*  Types
*********************************************************/
struct ZSTDn_DDict_s {
    void* dictBuffer;
    const void* dictContent;
    size_t dictSize;
    ZSTDn_entropyDTables_t entropy;
    U32 dictID;
    U32 entropyPresent;
    ZSTDn_customMem cMem;
};  /* typedef'd to ZSTDn_DDict within "zstd.h" */

const void* ZSTDn_DDict_dictContent(const ZSTDn_DDict* ddict)
{
    assert(ddict != NULL);
    return ddict->dictContent;
}

size_t ZSTDn_DDict_dictSize(const ZSTDn_DDict* ddict)
{
    assert(ddict != NULL);
    return ddict->dictSize;
}

void ZSTDn_copyDDictParameters(ZSTDn_DCtx* dctx, const ZSTDn_DDict* ddict)
{
    DEBUGLOG(4, "ZSTDn_copyDDictParameters");
    assert(dctx != NULL);
    assert(ddict != NULL);
    dctx->dictID = ddict->dictID;
    dctx->prefixStart = ddict->dictContent;
    dctx->virtualStart = ddict->dictContent;
    dctx->dictEnd = (const BYTE*)ddict->dictContent + ddict->dictSize;
    dctx->previousDstEnd = dctx->dictEnd;
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    dctx->dictContentBeginForFuzzing = dctx->prefixStart;
    dctx->dictContentEndForFuzzing = dctx->previousDstEnd;
#endif
    if (ddict->entropyPresent) {
        dctx->litEntropy = 1;
        dctx->fseEntropy = 1;
        dctx->LLTptr = ddict->entropy.LLTable;
        dctx->MLTptr = ddict->entropy.MLTable;
        dctx->OFTptr = ddict->entropy.OFTable;
        dctx->HUFptr = ddict->entropy.hufTable;
        dctx->entropy.rep[0] = ddict->entropy.rep[0];
        dctx->entropy.rep[1] = ddict->entropy.rep[1];
        dctx->entropy.rep[2] = ddict->entropy.rep[2];
    } else {
        dctx->litEntropy = 0;
        dctx->fseEntropy = 0;
    }
}


static size_t
ZSTDn_loadEntropy_intoDDict(ZSTDn_DDict* ddict,
                           ZSTDn_dictContentType_e dictContentType)
{
    ddict->dictID = 0;
    ddict->entropyPresent = 0;
    if (dictContentType == ZSTDn_dct_rawContent) return 0;

    if (ddict->dictSize < 8) {
        if (dictContentType == ZSTDn_dct_fullDict)
            return ERROR(dictionary_corrupted);   /* only accept specified dictionaries */
        return 0;   /* pure content mode */
    }
    {   U32 const magic = MEMn_readLE32(ddict->dictContent);
        if (magic != ZSTDn_MAGIC_DICTIONARY) {
            if (dictContentType == ZSTDn_dct_fullDict)
                return ERROR(dictionary_corrupted);   /* only accept specified dictionaries */
            return 0;   /* pure content mode */
        }
    }
    ddict->dictID = MEMn_readLE32((const char*)ddict->dictContent + ZSTDn_FRAMEIDSIZE);

    /* load entropy tables */
    RETURN_ERROR_IF(ZSTDn_isError(ZSTDn_loadDEntropy(
            &ddict->entropy, ddict->dictContent, ddict->dictSize)),
        dictionary_corrupted, "");
    ddict->entropyPresent = 1;
    return 0;
}


static size_t ZSTDn_initDDict_internal(ZSTDn_DDict* ddict,
                                      const void* dict, size_t dictSize,
                                      ZSTDn_dictLoadMethod_e dictLoadMethod,
                                      ZSTDn_dictContentType_e dictContentType)
{
    if ((dictLoadMethod == ZSTDn_dlm_byRef) || (!dict) || (!dictSize)) {
        ddict->dictBuffer = NULL;
        ddict->dictContent = dict;
        if (!dict) dictSize = 0;
    } else {
        void* const internalBuffer = ZSTDn_customMalloc(dictSize, ddict->cMem);
        ddict->dictBuffer = internalBuffer;
        ddict->dictContent = internalBuffer;
        if (!internalBuffer) return ERROR(memory_allocation);
        ZSTDn_memcpy(internalBuffer, dict, dictSize);
    }
    ddict->dictSize = dictSize;
    ddict->entropy.hufTable[0] = (HUFn_DTable)((HufLog)*0x1000001);  /* cover both little and big endian */

    /* parse dictionary content */
    FORWARD_IF_ERROR( ZSTDn_loadEntropy_intoDDict(ddict, dictContentType) , "");

    return 0;
}

ZSTDn_DDict* ZSTDn_createDDict_advanced(const void* dict, size_t dictSize,
                                      ZSTDn_dictLoadMethod_e dictLoadMethod,
                                      ZSTDn_dictContentType_e dictContentType,
                                      ZSTDn_customMem customMem)
{
    if ((!customMem.customAlloc) ^ (!customMem.customFree)) return NULL;

    {   ZSTDn_DDict* const ddict = (ZSTDn_DDict*) ZSTDn_customMalloc(sizeof(ZSTDn_DDict), customMem);
        if (ddict == NULL) return NULL;
        ddict->cMem = customMem;
        {   size_t const initResult = ZSTDn_initDDict_internal(ddict,
                                            dict, dictSize,
                                            dictLoadMethod, dictContentType);
            if (ZSTDn_isError(initResult)) {
                ZSTDn_freeDDict(ddict);
                return NULL;
        }   }
        return ddict;
    }
}

/*! ZSTDn_createDDict() :
*   Create a digested dictionary, to start decompression without startup delay.
*   `dict` content is copied inside DDict.
*   Consequently, `dict` can be released after `ZSTDn_DDict` creation */
ZSTDn_DDict* ZSTDn_createDDict(const void* dict, size_t dictSize)
{
    ZSTDn_customMem const allocator = { NULL, NULL, NULL };
    return ZSTDn_createDDict_advanced(dict, dictSize, ZSTDn_dlm_byCopy, ZSTDn_dct_auto, allocator);
}

/*! ZSTDn_createDDict_byReference() :
 *  Create a digested dictionary, to start decompression without startup delay.
 *  Dictionary content is simply referenced, it will be accessed during decompression.
 *  Warning : dictBuffer must outlive DDict (DDict must be freed before dictBuffer) */
ZSTDn_DDict* ZSTDn_createDDict_byReference(const void* dictBuffer, size_t dictSize)
{
    ZSTDn_customMem const allocator = { NULL, NULL, NULL };
    return ZSTDn_createDDict_advanced(dictBuffer, dictSize, ZSTDn_dlm_byRef, ZSTDn_dct_auto, allocator);
}


const ZSTDn_DDict* ZSTDn_initStaticDDict(
                                void* sBuffer, size_t sBufferSize,
                                const void* dict, size_t dictSize,
                                ZSTDn_dictLoadMethod_e dictLoadMethod,
                                ZSTDn_dictContentType_e dictContentType)
{
    size_t const neededSpace = sizeof(ZSTDn_DDict)
                             + (dictLoadMethod == ZSTDn_dlm_byRef ? 0 : dictSize);
    ZSTDn_DDict* const ddict = (ZSTDn_DDict*)sBuffer;
    assert(sBuffer != NULL);
    assert(dict != NULL);
    if ((size_t)sBuffer & 7) return NULL;   /* 8-aligned */
    if (sBufferSize < neededSpace) return NULL;
    if (dictLoadMethod == ZSTDn_dlm_byCopy) {
        ZSTDn_memcpy(ddict+1, dict, dictSize);  /* local copy */
        dict = ddict+1;
    }
    if (ZSTDn_isError( ZSTDn_initDDict_internal(ddict,
                                              dict, dictSize,
                                              ZSTDn_dlm_byRef, dictContentType) ))
        return NULL;
    return ddict;
}


size_t ZSTDn_freeDDict(ZSTDn_DDict* ddict)
{
    if (ddict==NULL) return 0;   /* support free on NULL */
    {   ZSTDn_customMem const cMem = ddict->cMem;
        ZSTDn_customFree(ddict->dictBuffer, cMem);
        ZSTDn_customFree(ddict, cMem);
        return 0;
    }
}

/*! ZSTDn_estimateDDictSize() :
 *  Estimate amount of memory that will be needed to create a dictionary for decompression.
 *  Note : dictionary created by reference using ZSTDn_dlm_byRef are smaller */
size_t ZSTDn_estimateDDictSize(size_t dictSize, ZSTDn_dictLoadMethod_e dictLoadMethod)
{
    return sizeof(ZSTDn_DDict) + (dictLoadMethod == ZSTDn_dlm_byRef ? 0 : dictSize);
}

size_t ZSTDn_sizeof_DDict(const ZSTDn_DDict* ddict)
{
    if (ddict==NULL) return 0;   /* support sizeof on NULL */
    return sizeof(*ddict) + (ddict->dictBuffer ? ddict->dictSize : 0) ;
}

/*! ZSTDn_getDictID_fromDDict() :
 *  Provides the dictID of the dictionary loaded into `ddict`.
 *  If @return == 0, the dictionary is not conformant to Zstandard specification, or empty.
 *  Non-conformant dictionaries can still be loaded, but as content-only dictionaries. */
unsigned ZSTDn_getDictID_fromDDict(const ZSTDn_DDict* ddict)
{
    if (ddict==NULL) return 0;
    return ZSTDn_getDictID_fromDict(ddict->dictContent, ddict->dictSize);
}
