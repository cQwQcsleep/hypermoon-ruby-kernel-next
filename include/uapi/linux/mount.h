/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Linux 5.2 moved the MS_* mount flags out of <linux/fs.h> into their own
 * uapi header.  On 4.19 they still live in <uapi/linux/fs.h>, so forward the
 * newer header name there for drivers that include <uapi/linux/mount.h>.
 */
#ifndef _UAPI_LINUX_MOUNT_H
#define _UAPI_LINUX_MOUNT_H

#include <linux/fs.h>

#endif /* _UAPI_LINUX_MOUNT_H */
