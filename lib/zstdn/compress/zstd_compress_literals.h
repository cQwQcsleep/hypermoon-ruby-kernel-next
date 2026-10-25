/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTDn_COMPRESS_LITERALS_H
#define ZSTDn_COMPRESS_LITERALS_H

#include "zstd_compress_internal.h" /* ZSTDn_hufCTables_t, ZSTDn_minGain() */


size_t ZSTDn_noCompressLiterals (void* dst, size_t dstCapacity, const void* src, size_t srcSize);

size_t ZSTDn_compressRleLiteralsBlock (void* dst, size_t dstCapacity, const void* src, size_t srcSize);

size_t ZSTDn_compressLiterals (ZSTDn_hufCTables_t const* prevHuf,
                              ZSTDn_hufCTables_t* nextHuf,
                              ZSTDn_strategy strategy, int disableLiteralCompression,
                              void* dst, size_t dstCapacity,
                        const void* src, size_t srcSize,
                              void* entropyWorkspace, size_t entropyWorkspaceSize,
                        const int bmi2);

#endif /* ZSTDn_COMPRESS_LITERALS_H */
