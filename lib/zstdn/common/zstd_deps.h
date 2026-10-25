/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * Copyright (c) Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/*
 * This file provides common libc dependencies that zstd requires.
 * The purpose is to allow replacing this file with a custom implementation
 * to compile zstd without libc support.
 */

/* Need:
 * NULL
 * INT_MAX
 * UINT_MAX
 * ZSTDn_memcpy()
 * ZSTDn_memset()
 * ZSTDn_memmove()
 */
#ifndef ZSTDn_DEPS_COMMON
#define ZSTDn_DEPS_COMMON

#include <linux/limits.h>
#include <linux/stddef.h>

#define ZSTDn_memcpy(d,s,n) __builtin_memcpy((d),(s),(n))
#define ZSTDn_memmove(d,s,n) __builtin_memmove((d),(s),(n))
#define ZSTDn_memset(d,s,n) __builtin_memset((d),(s),(n))

#endif /* ZSTDn_DEPS_COMMON */

/*
 * Define malloc as always failing. That means the user must
 * either use ZSTDn_customMem or statically allocate memory.
 * Need:
 * ZSTDn_malloc()
 * ZSTDn_free()
 * ZSTDn_calloc()
 */
#ifdef ZSTDn_DEPS_NEED_MALLOC
#ifndef ZSTDn_DEPS_MALLOC
#define ZSTDn_DEPS_MALLOC

#define ZSTDn_malloc(s) ({ (void)(s); NULL; })
#define ZSTDn_free(p) ((void)(p))
#define ZSTDn_calloc(n,s) ({ (void)(n); (void)(s); NULL; })

#endif /* ZSTDn_DEPS_MALLOC */
#endif /* ZSTDn_DEPS_NEED_MALLOC */

/*
 * Provides 64-bit math support.
 * Need:
 * U64 ZSTDn_div64(U64 dividend, U32 divisor)
 */
#ifdef ZSTDn_DEPS_NEED_MATH64
#ifndef ZSTDn_DEPS_MATH64
#define ZSTDn_DEPS_MATH64

#include <linux/math64.h>

static uint64_t ZSTDn_div64(uint64_t dividend, uint32_t divisor) {
  return div_u64(dividend, divisor);
}

#endif /* ZSTDn_DEPS_MATH64 */
#endif /* ZSTDn_DEPS_NEED_MATH64 */

/*
 * This is only requested when DEBUGLEVEL >= 1, meaning
 * it is disabled in production.
 * Need:
 * assert()
 */
#ifdef ZSTDn_DEPS_NEED_ASSERT
#ifndef ZSTDn_DEPS_ASSERT
#define ZSTDn_DEPS_ASSERT

#include <linux/kernel.h>

#define assert(x) WARN_ON((x))

#endif /* ZSTDn_DEPS_ASSERT */
#endif /* ZSTDn_DEPS_NEED_ASSERT */

/*
 * This is only requested when DEBUGLEVEL >= 2, meaning
 * it is disabled in production.
 * Need:
 * ZSTDn_DEBUG_PRINT()
 */
#ifdef ZSTDn_DEPS_NEED_IO
#ifndef ZSTDn_DEPS_IO
#define ZSTDn_DEPS_IO

#include <linux/printk.h>

#define ZSTDn_DEBUG_PRINT(...) pr_debug(__VA_ARGS__)

#endif /* ZSTDn_DEPS_IO */
#endif /* ZSTDn_DEPS_NEED_IO */

/*
 * Only requested when MSAN is enabled.
 * Need:
 * intptr_t
 */
#ifdef ZSTDn_DEPS_NEED_STDINT
#ifndef ZSTDn_DEPS_STDINT
#define ZSTDn_DEPS_STDINT

/*
 * The Linux Kernel doesn't provide intptr_t, only uintptr_t, which
 * is an unsigned long.
 */
typedef long intptr_t;

#endif /* ZSTDn_DEPS_STDINT */
#endif /* ZSTDn_DEPS_NEED_STDINT */
