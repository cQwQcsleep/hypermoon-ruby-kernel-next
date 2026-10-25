/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


#ifndef ZSTDn_DDICT_H
#define ZSTDn_DDICT_H

/*-*******************************************************
 *  Dependencies
 *********************************************************/
#include "../common/zstd_deps.h"   /* size_t */
#include "include/zstd.h"     /* ZSTDn_DDict, and several public functions */


/*-*******************************************************
 *  Interface
 *********************************************************/

/* note: several prototypes are already published in `zstd.h` :
 * ZSTDn_createDDict()
 * ZSTDn_createDDict_byReference()
 * ZSTDn_createDDict_advanced()
 * ZSTDn_freeDDict()
 * ZSTDn_initStaticDDict()
 * ZSTDn_sizeof_DDict()
 * ZSTDn_estimateDDictSize()
 * ZSTDn_getDictID_fromDict()
 */

const void* ZSTDn_DDict_dictContent(const ZSTDn_DDict* ddict);
size_t ZSTDn_DDict_dictSize(const ZSTDn_DDict* ddict);

void ZSTDn_copyDDictParameters(ZSTDn_DCtx* dctx, const ZSTDn_DDict* ddict);



#endif /* ZSTDn_DDICT_H */
