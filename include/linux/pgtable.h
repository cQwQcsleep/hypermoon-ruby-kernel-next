/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Linux 5.4 moved the generic page-table helpers into <linux/pgtable.h>.
 * This 4.19 tree keeps them in <asm/pgtable.h>; provide the newer header
 * name so drivers written against 5.x still build here.
 */
#ifndef _LINUX_PGTABLE_H
#define _LINUX_PGTABLE_H

#include <asm/pgtable.h>

#endif /* _LINUX_PGTABLE_H */
