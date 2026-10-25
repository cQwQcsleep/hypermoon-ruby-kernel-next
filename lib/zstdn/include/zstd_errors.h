/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTDn_ERRORS_H_398273423
#define ZSTDn_ERRORS_H_398273423


/*===== dependency =====*/
#include <linux/types.h>   /* size_t */


/* =====   ZSTDERRORLIB_API : control library symbols visibility   ===== */
#define ZSTDERRORLIB_VISIBILITY 
#define ZSTDERRORLIB_API ZSTDERRORLIB_VISIBILITY

/*-*********************************************
 *  Error codes list
 *-*********************************************
 *  Error codes _values_ are pinned down since v1.3.1 only.
 *  Therefore, don't rely on values if you may link to any version < v1.3.1.
 *
 *  Only values < 100 are considered stable.
 *
 *  note 1 : this API shall be used with static linking only.
 *           dynamic linking is not yet officially supported.
 *  note 2 : Prefer relying on the enum than on its value whenever possible
 *           This is the only supported way to use the error list < v1.3.1
 *  note 3 : ZSTDn_isError() is always correct, whatever the library version.
 **********************************************/
typedef enum {
  ZSTDn_error_no_error = 0,
  ZSTDn_error_GENERIC  = 1,
  ZSTDn_error_prefix_unknown                = 10,
  ZSTDn_error_version_unsupported           = 12,
  ZSTDn_error_frameParameter_unsupported    = 14,
  ZSTDn_error_frameParameter_windowTooLarge = 16,
  ZSTDn_error_corruption_detected = 20,
  ZSTDn_error_checksum_wrong      = 22,
  ZSTDn_error_dictionary_corrupted      = 30,
  ZSTDn_error_dictionary_wrong          = 32,
  ZSTDn_error_dictionaryCreation_failed = 34,
  ZSTDn_error_parameter_unsupported   = 40,
  ZSTDn_error_parameter_outOfBound    = 42,
  ZSTDn_error_tableLog_tooLarge       = 44,
  ZSTDn_error_maxSymbolValue_tooLarge = 46,
  ZSTDn_error_maxSymbolValue_tooSmall = 48,
  ZSTDn_error_stage_wrong       = 60,
  ZSTDn_error_init_missing      = 62,
  ZSTDn_error_memory_allocation = 64,
  ZSTDn_error_workSpace_tooSmall= 66,
  ZSTDn_error_dstSize_tooSmall = 70,
  ZSTDn_error_srcSize_wrong    = 72,
  ZSTDn_error_dstBuffer_null   = 74,
  /* following error codes are __NOT STABLE__, they can be removed or changed in future versions */
  ZSTDn_error_frameIndex_tooLarge = 100,
  ZSTDn_error_seekableIO          = 102,
  ZSTDn_error_dstBuffer_wrong     = 104,
  ZSTDn_error_srcBuffer_wrong     = 105,
  ZSTDn_error_maxCode = 120  /* never EVER use this value directly, it can change in future versions! Use ZSTDn_isError() instead */
} ZSTDn_ErrorCode;

/*! ZSTDn_getErrorCode() :
    convert a `size_t` function result into a `ZSTDn_ErrorCode` enum type,
    which can be used to compare with enum list published above */
ZSTDERRORLIB_API ZSTDn_ErrorCode ZSTDn_getErrorCode(size_t functionResult);
ZSTDERRORLIB_API const char* ZSTDn_getErrorString(ZSTDn_ErrorCode code);   /*< Same as ZSTDn_getErrorName, but using a `ZSTDn_ErrorCode` enum argument */



#endif /* ZSTDn_ERRORS_H_398273423 */
