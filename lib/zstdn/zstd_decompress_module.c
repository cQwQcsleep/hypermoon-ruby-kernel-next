// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (c) Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include "include/zstd.h"

#include "common/zstd_deps.h"

/* Common symbols. zstdn_compress must depend on zstdn_decompress. */

unsigned int zstdn_is_error(size_t code)
{
	return ZSTDn_isError(code);
}

zstdn_error_code zstdn_get_error_code(size_t code)
{
	return ZSTDn_getErrorCode(code);
}

const char *zstdn_get_error_name(size_t code)
{
	return ZSTDn_getErrorName(code);
}

/* Decompression symbols. */

size_t zstdn_dctx_workspace_bound(void)
{
	return ZSTDn_estimateDCtxSize();
}

zstdn_dctx *zstdn_init_dctx(void *workspace, size_t workspace_size)
{
	if (workspace == NULL)
		return NULL;
	return ZSTDn_initStaticDCtx(workspace, workspace_size);
}

size_t zstdn_decompress_dctx(zstdn_dctx *dctx, void *dst, size_t dst_capacity,
	const void *src, size_t src_size)
{
	return ZSTDn_decompressDCtx(dctx, dst, dst_capacity, src, src_size);
}

size_t zstdn_dstream_workspace_bound(size_t max_window_size)
{
	return ZSTDn_estimateDStreamSize(max_window_size);
}

zstdn_dstream *zstdn_init_dstream(size_t max_window_size, void *workspace,
	size_t workspace_size)
{
	if (workspace == NULL)
		return NULL;
	(void)max_window_size;
	return ZSTDn_initStaticDStream(workspace, workspace_size);
}

size_t zstdn_reset_dstream(zstdn_dstream *dstream)
{
	return ZSTDn_resetDStream(dstream);
}

size_t zstdn_decompress_stream(zstdn_dstream *dstream, zstdn_out_buffer *output,
	zstdn_in_buffer *input)
{
	return ZSTDn_decompressStream(dstream, output, input);
}

size_t zstdn_find_frame_compressed_size(const void *src, size_t src_size)
{
	return ZSTDn_findFrameCompressedSize(src, src_size);
}

size_t zstdn_get_frame_header(zstdn_frame_header *header, const void *src,
	size_t src_size)
{
	return ZSTDn_getFrameHeader(header, src, src_size);
}

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Zstd Decompressor");
