/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Linux 5.3 split min()/max()/clamp() out of <linux/kernel.h> into this
 * header.  On 4.19 they still live in <linux/kernel.h>, so forward to it for
 * drivers that include <linux/minmax.h> directly.
 */
#ifndef _LINUX_MINMAX_H
#define _LINUX_MINMAX_H

#include <linux/kernel.h>

#endif /* _LINUX_MINMAX_H */
