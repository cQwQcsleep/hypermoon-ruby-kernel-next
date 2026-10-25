/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTDn_CWKSP_H
#define ZSTDn_CWKSP_H

/*-*************************************
*  Dependencies
***************************************/
#include "../common/zstd_internal.h"


/*-*************************************
*  Constants
***************************************/

/* Since the workspace is effectively its own little malloc implementation /
 * arena, when we run under ASAN, we should similarly insert redzones between
 * each internal element of the workspace, so ASAN will catch overruns that
 * reach outside an object but that stay inside the workspace.
 *
 * This defines the size of that redzone.
 */
#ifndef ZSTDn_CWKSP_ASAN_REDZONE_SIZE
#define ZSTDn_CWKSP_ASAN_REDZONE_SIZE 128
#endif

/*-*************************************
*  Structures
***************************************/
typedef enum {
    ZSTDn_cwksp_alloc_objects,
    ZSTDn_cwksp_alloc_buffers,
    ZSTDn_cwksp_alloc_aligned
} ZSTDn_cwksp_alloc_phase_e;

/*
 * Used to describe whether the workspace is statically allocated (and will not
 * necessarily ever be freed), or if it's dynamically allocated and we can
 * expect a well-formed caller to free this.
 */
typedef enum {
    ZSTDn_cwksp_dynamic_alloc,
    ZSTDn_cwksp_static_alloc
} ZSTDn_cwksp_static_alloc_e;

/*
 * Zstd fits all its internal datastructures into a single continuous buffer,
 * so that it only needs to perform a single OS allocation (or so that a buffer
 * can be provided to it and it can perform no allocations at all). This buffer
 * is called the workspace.
 *
 * Several optimizations complicate that process of allocating memory ranges
 * from this workspace for each internal datastructure:
 *
 * - These different internal datastructures have different setup requirements:
 *
 *   - The static objects need to be cleared once and can then be trivially
 *     reused for each compression.
 *
 *   - Various buffers don't need to be initialized at all--they are always
 *     written into before they're read.
 *
 *   - The matchstate tables have a unique requirement that they don't need
 *     their memory to be totally cleared, but they do need the memory to have
 *     some bound, i.e., a guarantee that all values in the memory they've been
 *     allocated is less than some maximum value (which is the starting value
 *     for the indices that they will then use for compression). When this
 *     guarantee is provided to them, they can use the memory without any setup
 *     work. When it can't, they have to clear the area.
 *
 * - These buffers also have different alignment requirements.
 *
 * - We would like to reuse the objects in the workspace for multiple
 *   compressions without having to perform any expensive reallocation or
 *   reinitialization work.
 *
 * - We would like to be able to efficiently reuse the workspace across
 *   multiple compressions **even when the compression parameters change** and
 *   we need to resize some of the objects (where possible).
 *
 * To attempt to manage this buffer, given these constraints, the ZSTDn_cwksp
 * abstraction was created. It works as follows:
 *
 * Workspace Layout:
 *
 * [                        ... workspace ...                         ]
 * [objects][tables ... ->] free space [<- ... aligned][<- ... buffers]
 *
 * The various objects that live in the workspace are divided into the
 * following categories, and are allocated separately:
 *
 * - Static objects: this is optionally the enclosing ZSTDn_CCtx or ZSTDn_CDict,
 *   so that literally everything fits in a single buffer. Note: if present,
 *   this must be the first object in the workspace, since ZSTDn_customFree{CCtx,
 *   CDict}() rely on a pointer comparison to see whether one or two frees are
 *   required.
 *
 * - Fixed size objects: these are fixed-size, fixed-count objects that are
 *   nonetheless "dynamically" allocated in the workspace so that we can
 *   control how they're initialized separately from the broader ZSTDn_CCtx.
 *   Examples:
 *   - Entropy Workspace
 *   - 2 x ZSTDn_compressedBlockState_t
 *   - CDict dictionary contents
 *
 * - Tables: these are any of several different datastructures (hash tables,
 *   chain tables, binary trees) that all respect a common format: they are
 *   uint32_t arrays, all of whose values are between 0 and (nextSrc - base).
 *   Their sizes depend on the cparams.
 *
 * - Aligned: these buffers are used for various purposes that require 4 byte
 *   alignment, but don't require any initialization before they're used.
 *
 * - Buffers: these buffers are used for various purposes that don't require
 *   any alignment or initialization before they're used. This means they can
 *   be moved around at no cost for a new compression.
 *
 * Allocating Memory:
 *
 * The various types of objects must be allocated in order, so they can be
 * correctly packed into the workspace buffer. That order is:
 *
 * 1. Objects
 * 2. Buffers
 * 3. Aligned
 * 4. Tables
 *
 * Attempts to reserve objects of different types out of order will fail.
 */
typedef struct {
    void* workspace;
    void* workspaceEnd;

    void* objectEnd;
    void* tableEnd;
    void* tableValidEnd;
    void* allocStart;

    BYTE allocFailed;
    int workspaceOversizedDuration;
    ZSTDn_cwksp_alloc_phase_e phase;
    ZSTDn_cwksp_static_alloc_e isStatic;
} ZSTDn_cwksp;

/*-*************************************
*  Functions
***************************************/

MEMn_STATIC size_t ZSTDn_cwksp_available_space(ZSTDn_cwksp* ws);

MEMn_STATIC void ZSTDn_cwksp_assert_internal_consistency(ZSTDn_cwksp* ws) {
    (void)ws;
    assert(ws->workspace <= ws->objectEnd);
    assert(ws->objectEnd <= ws->tableEnd);
    assert(ws->objectEnd <= ws->tableValidEnd);
    assert(ws->tableEnd <= ws->allocStart);
    assert(ws->tableValidEnd <= ws->allocStart);
    assert(ws->allocStart <= ws->workspaceEnd);
}

/*
 * Align must be a power of 2.
 */
MEMn_STATIC size_t ZSTDn_cwksp_align(size_t size, size_t const align) {
    size_t const mask = align - 1;
    assert((align & mask) == 0);
    return (size + mask) & ~mask;
}

/*
 * Use this to determine how much space in the workspace we will consume to
 * allocate this object. (Normally it should be exactly the size of the object,
 * but under special conditions, like ASAN, where we pad each object, it might
 * be larger.)
 *
 * Since tables aren't currently redzoned, you don't need to call through this
 * to figure out how much space you need for the matchState tables. Everything
 * else is though.
 */
MEMn_STATIC size_t ZSTDn_cwksp_alloc_size(size_t size) {
    if (size == 0)
        return 0;
    return size;
}

MEMn_STATIC void ZSTDn_cwksp_internal_advance_phase(
        ZSTDn_cwksp* ws, ZSTDn_cwksp_alloc_phase_e phase) {
    assert(phase >= ws->phase);
    if (phase > ws->phase) {
        if (ws->phase < ZSTDn_cwksp_alloc_buffers &&
                phase >= ZSTDn_cwksp_alloc_buffers) {
            ws->tableValidEnd = ws->objectEnd;
        }
        if (ws->phase < ZSTDn_cwksp_alloc_aligned &&
                phase >= ZSTDn_cwksp_alloc_aligned) {
            /* If unaligned allocations down from a too-large top have left us
             * unaligned, we need to realign our alloc ptr. Technically, this
             * can consume space that is unaccounted for in the neededSpace
             * calculation. However, I believe this can only happen when the
             * workspace is too large, and specifically when it is too large
             * by a larger margin than the space that will be consumed. */
            /* TODO: cleaner, compiler warning friendly way to do this??? */
            ws->allocStart = (BYTE*)ws->allocStart - ((size_t)ws->allocStart & (sizeof(U32)-1));
            if (ws->allocStart < ws->tableValidEnd) {
                ws->tableValidEnd = ws->allocStart;
            }
        }
        ws->phase = phase;
    }
}

/*
 * Returns whether this object/buffer/etc was allocated in this workspace.
 */
MEMn_STATIC int ZSTDn_cwksp_owns_buffer(const ZSTDn_cwksp* ws, const void* ptr) {
    return (ptr != NULL) && (ws->workspace <= ptr) && (ptr <= ws->workspaceEnd);
}

/*
 * Internal function. Do not use directly.
 */
MEMn_STATIC void* ZSTDn_cwksp_reserve_internal(
        ZSTDn_cwksp* ws, size_t bytes, ZSTDn_cwksp_alloc_phase_e phase) {
    void* alloc;
    void* bottom = ws->tableEnd;
    ZSTDn_cwksp_internal_advance_phase(ws, phase);
    alloc = (BYTE *)ws->allocStart - bytes;

    if (bytes == 0)
        return NULL;


    DEBUGLOG(5, "cwksp: reserving %p %zd bytes, %zd bytes remaining",
        alloc, bytes, ZSTDn_cwksp_available_space(ws) - bytes);
    ZSTDn_cwksp_assert_internal_consistency(ws);
    assert(alloc >= bottom);
    if (alloc < bottom) {
        DEBUGLOG(4, "cwksp: alloc failed!");
        ws->allocFailed = 1;
        return NULL;
    }
    if (alloc < ws->tableValidEnd) {
        ws->tableValidEnd = alloc;
    }
    ws->allocStart = alloc;


    return alloc;
}

/*
 * Reserves and returns unaligned memory.
 */
MEMn_STATIC BYTE* ZSTDn_cwksp_reserve_buffer(ZSTDn_cwksp* ws, size_t bytes) {
    return (BYTE*)ZSTDn_cwksp_reserve_internal(ws, bytes, ZSTDn_cwksp_alloc_buffers);
}

/*
 * Reserves and returns memory sized on and aligned on sizeof(unsigned).
 */
MEMn_STATIC void* ZSTDn_cwksp_reserve_aligned(ZSTDn_cwksp* ws, size_t bytes) {
    assert((bytes & (sizeof(U32)-1)) == 0);
    return ZSTDn_cwksp_reserve_internal(ws, ZSTDn_cwksp_align(bytes, sizeof(U32)), ZSTDn_cwksp_alloc_aligned);
}

/*
 * Aligned on sizeof(unsigned). These buffers have the special property that
 * their values remain constrained, allowing us to re-use them without
 * memset()-ing them.
 */
MEMn_STATIC void* ZSTDn_cwksp_reserve_table(ZSTDn_cwksp* ws, size_t bytes) {
    const ZSTDn_cwksp_alloc_phase_e phase = ZSTDn_cwksp_alloc_aligned;
    void* alloc = ws->tableEnd;
    void* end = (BYTE *)alloc + bytes;
    void* top = ws->allocStart;

    DEBUGLOG(5, "cwksp: reserving %p table %zd bytes, %zd bytes remaining",
        alloc, bytes, ZSTDn_cwksp_available_space(ws) - bytes);
    assert((bytes & (sizeof(U32)-1)) == 0);
    ZSTDn_cwksp_internal_advance_phase(ws, phase);
    ZSTDn_cwksp_assert_internal_consistency(ws);
    assert(end <= top);
    if (end > top) {
        DEBUGLOG(4, "cwksp: table alloc failed!");
        ws->allocFailed = 1;
        return NULL;
    }
    ws->tableEnd = end;


    return alloc;
}

/*
 * Aligned on sizeof(void*).
 */
MEMn_STATIC void* ZSTDn_cwksp_reserve_object(ZSTDn_cwksp* ws, size_t bytes) {
    size_t roundedBytes = ZSTDn_cwksp_align(bytes, sizeof(void*));
    void* alloc = ws->objectEnd;
    void* end = (BYTE*)alloc + roundedBytes;


    DEBUGLOG(5,
        "cwksp: reserving %p object %zd bytes (rounded to %zd), %zd bytes remaining",
        alloc, bytes, roundedBytes, ZSTDn_cwksp_available_space(ws) - roundedBytes);
    assert(((size_t)alloc & (sizeof(void*)-1)) == 0);
    assert((bytes & (sizeof(void*)-1)) == 0);
    ZSTDn_cwksp_assert_internal_consistency(ws);
    /* we must be in the first phase, no advance is possible */
    if (ws->phase != ZSTDn_cwksp_alloc_objects || end > ws->workspaceEnd) {
        DEBUGLOG(4, "cwksp: object alloc failed!");
        ws->allocFailed = 1;
        return NULL;
    }
    ws->objectEnd = end;
    ws->tableEnd = end;
    ws->tableValidEnd = end;


    return alloc;
}

MEMn_STATIC void ZSTDn_cwksp_mark_tables_dirty(ZSTDn_cwksp* ws) {
    DEBUGLOG(4, "cwksp: ZSTDn_cwksp_mark_tables_dirty");


    assert(ws->tableValidEnd >= ws->objectEnd);
    assert(ws->tableValidEnd <= ws->allocStart);
    ws->tableValidEnd = ws->objectEnd;
    ZSTDn_cwksp_assert_internal_consistency(ws);
}

MEMn_STATIC void ZSTDn_cwksp_mark_tables_clean(ZSTDn_cwksp* ws) {
    DEBUGLOG(4, "cwksp: ZSTDn_cwksp_mark_tables_clean");
    assert(ws->tableValidEnd >= ws->objectEnd);
    assert(ws->tableValidEnd <= ws->allocStart);
    if (ws->tableValidEnd < ws->tableEnd) {
        ws->tableValidEnd = ws->tableEnd;
    }
    ZSTDn_cwksp_assert_internal_consistency(ws);
}

/*
 * Zero the part of the allocated tables not already marked clean.
 */
MEMn_STATIC void ZSTDn_cwksp_clean_tables(ZSTDn_cwksp* ws) {
    DEBUGLOG(4, "cwksp: ZSTDn_cwksp_clean_tables");
    assert(ws->tableValidEnd >= ws->objectEnd);
    assert(ws->tableValidEnd <= ws->allocStart);
    if (ws->tableValidEnd < ws->tableEnd) {
        ZSTDn_memset(ws->tableValidEnd, 0, (BYTE*)ws->tableEnd - (BYTE*)ws->tableValidEnd);
    }
    ZSTDn_cwksp_mark_tables_clean(ws);
}

/*
 * Invalidates table allocations.
 * All other allocations remain valid.
 */
MEMn_STATIC void ZSTDn_cwksp_clear_tables(ZSTDn_cwksp* ws) {
    DEBUGLOG(4, "cwksp: clearing tables!");


    ws->tableEnd = ws->objectEnd;
    ZSTDn_cwksp_assert_internal_consistency(ws);
}

/*
 * Invalidates all buffer, aligned, and table allocations.
 * Object allocations remain valid.
 */
MEMn_STATIC void ZSTDn_cwksp_clear(ZSTDn_cwksp* ws) {
    DEBUGLOG(4, "cwksp: clearing!");



    ws->tableEnd = ws->objectEnd;
    ws->allocStart = ws->workspaceEnd;
    ws->allocFailed = 0;
    if (ws->phase > ZSTDn_cwksp_alloc_buffers) {
        ws->phase = ZSTDn_cwksp_alloc_buffers;
    }
    ZSTDn_cwksp_assert_internal_consistency(ws);
}

/*
 * The provided workspace takes ownership of the buffer [start, start+size).
 * Any existing values in the workspace are ignored (the previously managed
 * buffer, if present, must be separately freed).
 */
MEMn_STATIC void ZSTDn_cwksp_init(ZSTDn_cwksp* ws, void* start, size_t size, ZSTDn_cwksp_static_alloc_e isStatic) {
    DEBUGLOG(4, "cwksp: init'ing workspace with %zd bytes", size);
    assert(((size_t)start & (sizeof(void*)-1)) == 0); /* ensure correct alignment */
    ws->workspace = start;
    ws->workspaceEnd = (BYTE*)start + size;
    ws->objectEnd = ws->workspace;
    ws->tableValidEnd = ws->objectEnd;
    ws->phase = ZSTDn_cwksp_alloc_objects;
    ws->isStatic = isStatic;
    ZSTDn_cwksp_clear(ws);
    ws->workspaceOversizedDuration = 0;
    ZSTDn_cwksp_assert_internal_consistency(ws);
}

MEMn_STATIC size_t ZSTDn_cwksp_create(ZSTDn_cwksp* ws, size_t size, ZSTDn_customMem customMem) {
    void* workspace = ZSTDn_customMalloc(size, customMem);
    DEBUGLOG(4, "cwksp: creating new workspace with %zd bytes", size);
    RETURN_ERROR_IF(workspace == NULL, memory_allocation, "NULL pointer!");
    ZSTDn_cwksp_init(ws, workspace, size, ZSTDn_cwksp_dynamic_alloc);
    return 0;
}

MEMn_STATIC void ZSTDn_cwksp_free(ZSTDn_cwksp* ws, ZSTDn_customMem customMem) {
    void *ptr = ws->workspace;
    DEBUGLOG(4, "cwksp: freeing workspace");
    ZSTDn_memset(ws, 0, sizeof(ZSTDn_cwksp));
    ZSTDn_customFree(ptr, customMem);
}

/*
 * Moves the management of a workspace from one cwksp to another. The src cwksp
 * is left in an invalid state (src must be re-init()'ed before it's used again).
 */
MEMn_STATIC void ZSTDn_cwksp_move(ZSTDn_cwksp* dst, ZSTDn_cwksp* src) {
    *dst = *src;
    ZSTDn_memset(src, 0, sizeof(ZSTDn_cwksp));
}

MEMn_STATIC size_t ZSTDn_cwksp_sizeof(const ZSTDn_cwksp* ws) {
    return (size_t)((BYTE*)ws->workspaceEnd - (BYTE*)ws->workspace);
}

MEMn_STATIC size_t ZSTDn_cwksp_used(const ZSTDn_cwksp* ws) {
    return (size_t)((BYTE*)ws->tableEnd - (BYTE*)ws->workspace)
         + (size_t)((BYTE*)ws->workspaceEnd - (BYTE*)ws->allocStart);
}

MEMn_STATIC int ZSTDn_cwksp_reserve_failed(const ZSTDn_cwksp* ws) {
    return ws->allocFailed;
}

/*-*************************************
*  Functions Checking Free Space
***************************************/

MEMn_STATIC size_t ZSTDn_cwksp_available_space(ZSTDn_cwksp* ws) {
    return (size_t)((BYTE*)ws->allocStart - (BYTE*)ws->tableEnd);
}

MEMn_STATIC int ZSTDn_cwksp_check_available(ZSTDn_cwksp* ws, size_t additionalNeededSpace) {
    return ZSTDn_cwksp_available_space(ws) >= additionalNeededSpace;
}

MEMn_STATIC int ZSTDn_cwksp_check_too_large(ZSTDn_cwksp* ws, size_t additionalNeededSpace) {
    return ZSTDn_cwksp_check_available(
        ws, additionalNeededSpace * ZSTDn_WORKSPACETOOLARGE_FACTOR);
}

MEMn_STATIC int ZSTDn_cwksp_check_wasteful(ZSTDn_cwksp* ws, size_t additionalNeededSpace) {
    return ZSTDn_cwksp_check_too_large(ws, additionalNeededSpace)
        && ws->workspaceOversizedDuration > ZSTDn_WORKSPACETOOLARGE_MAXDURATION;
}

MEMn_STATIC void ZSTDn_cwksp_bump_oversized_duration(
        ZSTDn_cwksp* ws, size_t additionalNeededSpace) {
    if (ZSTDn_cwksp_check_too_large(ws, additionalNeededSpace)) {
        ws->workspaceOversizedDuration++;
    } else {
        ws->workspaceOversizedDuration = 0;
    }
}


#endif /* ZSTDn_CWKSP_H */
