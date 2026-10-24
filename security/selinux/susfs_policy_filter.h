/* SPDX-License-Identifier: GPL-2.0 */
/*
 * SELinux policy query filter for KernelSU/SUSFS.
 *
 * Detection apps ask selinuxfs and procattr about the extra types a root
 * solution injects (su, ksu_file, magisk, lsposed_file, ...). Answer
 * unprivileged callers as a stock kernel would: the type is unknown.
 */
#ifndef _SELINUX_SUSFS_POLICY_FILTER_H
#define _SELINUX_SUSFS_POLICY_FILTER_H

#include <linux/kernel.h>
#include <linux/cred.h>
#include <linux/string.h>
#include <linux/types.h>

#ifdef CONFIG_KSU_SUSFS
static inline bool susfs_policy_type_is_hidden(const char *type, size_t len)
{
	static const char * const names[] = {
		"su", "supersu", "magisk", "magisk_file", "ksu", "ksu_file",
		"lsposed_file", "riru_file", "droidspacesd", "msd_app",
		"msd_daemon", "xposed_data",
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(names); i++) {
		if (strlen(names[i]) == len && !memcmp(type, names[i], len))
			return true;
	}
	return false;
}

static inline bool susfs_selinuxfs_request_mentions_hidden(const char *buf,
							   size_t size)
{
	size_t i;

	if (uid_eq(current_uid(), GLOBAL_ROOT_UID))
		return false;

	for (i = 0; i + 1 < size; i++) {
		size_t j;

		if (buf[i] != ':')
			continue;
		j = i + 1;
		while (j < size && buf[j] && buf[j] != ':' && buf[j] != ' ' &&
		       buf[j] != '\n' && buf[j] != '\t')
			j++;
		if (susfs_policy_type_is_hidden(buf + i + 1, j - (i + 1)))
			return true;
	}
	return false;
}
#endif /* CONFIG_KSU_SUSFS */

#endif /* _SELINUX_SUSFS_POLICY_FILTER_H */
