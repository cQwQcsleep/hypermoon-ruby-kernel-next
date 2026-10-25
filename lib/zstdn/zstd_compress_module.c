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
#include "common/zstd_internal.h"

#define ZSTDn_FORWARD_IF_ERR(ret)            \
	do {                                \
		size_t const __ret = (ret); \
		if (ZSTDn_isError(__ret))    \
			return __ret;       \
	} while (0)

static size_t zstdn_cctx_init(zstdn_cctx *cctx, const zstdn_parameters *parameters,
	unsigned long long pledged_src_size)
{
	ZSTDn_FORWARD_IF_ERR(ZSTDn_CCtx_reset(
		cctx, ZSTDn_reset_session_and_parameters));
	ZSTDn_FORWARD_IF_ERR(ZSTDn_CCtx_setPledgedSrcSize(
		cctx, pledged_src_size));
	ZSTDn_FORWARD_IF_ERR(ZSTDn_CCtx_setParameter(
		cctx, ZSTDn_c_windowLog, parameters->cParams.windowLog));
	ZSTDn_FORWARD_IF_ERR(ZSTDn_CCtx_setParameter(
		cctx, ZSTDn_c_hashLog, parameters->cParams.hashLog));
	ZSTDn_FORWARD_IF_ERR(ZSTDn_CCtx_setParameter(
		cctx, ZSTDn_c_chainLog, parameters->cParams.chainLog));
	ZSTDn_FORWARD_IF_ERR(ZSTDn_CCtx_setParameter(
		cctx, ZSTDn_c_searchLog, parameters->cParams.searchLog));
	ZSTDn_FORWARD_IF_ERR(ZSTDn_CCtx_setParameter(
		cctx, ZSTDn_c_minMatch, parameters->cParams.minMatch));
	ZSTDn_FORWARD_IF_ERR(ZSTDn_CCtx_setParameter(
		cctx, ZSTDn_c_targetLength, parameters->cParams.targetLength));
	ZSTDn_FORWARD_IF_ERR(ZSTDn_CCtx_setParameter(
		cctx, ZSTDn_c_strategy, parameters->cParams.strategy));
	ZSTDn_FORWARD_IF_ERR(ZSTDn_CCtx_setParameter(
		cctx, ZSTDn_c_contentSizeFlag, parameters->fParams.contentSizeFlag));
	ZSTDn_FORWARD_IF_ERR(ZSTDn_CCtx_setParameter(
		cctx, ZSTDn_c_checksumFlag, parameters->fParams.checksumFlag));
	ZSTDn_FORWARD_IF_ERR(ZSTDn_CCtx_setParameter(
		cctx, ZSTDn_c_dictIDFlag, !parameters->fParams.noDictIDFlag));
	return 0;
}

int zstdn_min_clevel(void)
{
	return ZSTDn_minCLevel();
}

int zstdn_max_clevel(void)
{
	return ZSTDn_maxCLevel();
}

size_t zstdn_compress_bound(size_t src_size)
{
	return ZSTDn_compressBound(src_size);
}

zstdn_parameters zstdn_get_params(int level,
	unsigned long long estimated_src_size)
{
	return ZSTDn_getParams(level, estimated_src_size, 0);
}

size_t zstdn_cctx_workspace_bound(const zstdn_compression_parameters *cparams)
{
	return ZSTDn_estimateCCtxSize_usingCParams(*cparams);
}

zstdn_cctx *zstdn_init_cctx(void *workspace, size_t workspace_size)
{
	if (workspace == NULL)
		return NULL;
	return ZSTDn_initStaticCCtx(workspace, workspace_size);
}

size_t zstdn_compress_cctx(zstdn_cctx *cctx, void *dst, size_t dst_capacity,
	const void *src, size_t src_size, const zstdn_parameters *parameters)
{
	ZSTDn_FORWARD_IF_ERR(zstdn_cctx_init(cctx, parameters, src_size));
	return ZSTDn_compress2(cctx, dst, dst_capacity, src, src_size);
}

size_t zstdn_cstream_workspace_bound(const zstdn_compression_parameters *cparams)
{
	return ZSTDn_estimateCStreamSize_usingCParams(*cparams);
}

zstdn_cstream *zstdn_init_cstream(const zstdn_parameters *parameters,
	unsigned long long pledged_src_size, void *workspace, size_t workspace_size)
{
	zstdn_cstream *cstream;

	if (workspace == NULL)
		return NULL;

	cstream = ZSTDn_initStaticCStream(workspace, workspace_size);
	if (cstream == NULL)
		return NULL;

	/* 0 means unknown in linux zstd API but means 0 in new zstd API */
	if (pledged_src_size == 0)
		pledged_src_size = ZSTDn_CONTENTSIZE_UNKNOWN;

	if (ZSTDn_isError(zstdn_cctx_init(cstream, parameters, pledged_src_size)))
		return NULL;

	return cstream;
}

size_t zstdn_reset_cstream(zstdn_cstream *cstream,
	unsigned long long pledged_src_size)
{
	return ZSTDn_resetCStream(cstream, pledged_src_size);
}

size_t zstdn_compress_stream(zstdn_cstream *cstream, zstdn_out_buffer *output,
	zstdn_in_buffer *input)
{
	return ZSTDn_compressStream(cstream, output, input);
}

size_t zstdn_flush_stream(zstdn_cstream *cstream, zstdn_out_buffer *output)
{
	return ZSTDn_flushStream(cstream, output);
}

size_t zstdn_end_stream(zstdn_cstream *cstream, zstdn_out_buffer *output)
{
	return ZSTDn_endStream(cstream, output);
}

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Zstd Compressor");
