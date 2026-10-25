/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */



/*-*************************************
*  Dependencies
***************************************/
#define ZSTDn_DEPS_NEED_MALLOC
#include "zstd_deps.h"   /* ZSTDn_malloc, ZSTDn_calloc, ZSTDn_free, ZSTDn_memset */
#include "error_private.h"
#include "zstd_internal.h"


/*-****************************************
*  Version
******************************************/
unsigned ZSTDn_versionNumber(void) { return ZSTDn_VERSION_NUMBER; }

const char* ZSTDn_versionString(void) { return ZSTDn_VERSION_STRING; }


/*-****************************************
*  ZSTD Error Management
******************************************/
#undef ZSTDn_isError   /* defined within zstdn_internal.h */
/*! ZSTDn_isError() :
 *  tells if a return value is an error code
 *  symbol is required for external callers */
unsigned ZSTDn_isError(size_t code) { return ERR_isError(code); }

/*! ZSTDn_getErrorName() :
 *  provides error code string from function result (useful for debugging) */
const char* ZSTDn_getErrorName(size_t code) { return ERR_getErrorName(code); }

/*! ZSTDn_getError() :
 *  convert a `size_t` function result into a proper ZSTDn_errorCode enum */
ZSTDn_ErrorCode ZSTDn_getErrorCode(size_t code) { return ERR_getErrorCode(code); }

/*! ZSTDn_getErrorString() :
 *  provides error code string from enum */
const char* ZSTDn_getErrorString(ZSTDn_ErrorCode code) { return ERR_getErrorString(code); }



/*=**************************************************************
*  Custom allocator
****************************************************************/
void* ZSTDn_customMalloc(size_t size, ZSTDn_customMem customMem)
{
    if (customMem.customAlloc)
        return customMem.customAlloc(customMem.opaque, size);
    return ZSTDn_malloc(size);
}

void* ZSTDn_customCalloc(size_t size, ZSTDn_customMem customMem)
{
    if (customMem.customAlloc) {
        /* calloc implemented as malloc+memset;
         * not as efficient as calloc, but next best guess for custom malloc */
        void* const ptr = customMem.customAlloc(customMem.opaque, size);
        ZSTDn_memset(ptr, 0, size);
        return ptr;
    }
    return ZSTDn_calloc(1, size);
}

void ZSTDn_customFree(void* ptr, ZSTDn_customMem customMem)
{
    if (ptr!=NULL) {
        if (customMem.customFree)
            customMem.customFree(customMem.opaque, ptr);
        else
            ZSTDn_free(ptr);
    }
}
