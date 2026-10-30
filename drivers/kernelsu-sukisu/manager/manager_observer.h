#ifndef __KSU_H_MANAGER_OBSERVER
#define __KSU_H_MANAGER_OBSERVER

#include <linux/version.h>

/*
 * The package observer relies on the fsnotify inode-event API (5.9+) and
 * TWA_RESUME, so it is compiled out below 6.8 (see Kbuild); on those kernels
 * package tracking goes through manager/throne_tracker.c instead.
 */
#if defined(CONFIG_KSU_DISABLE_MANAGER) || \
	(LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0))
static inline int ksu_observer_init(void)
{
    return 0;
}

static inline void ksu_observer_exit(void)
{
}
#else
int ksu_observer_init(void);
void ksu_observer_exit(void);
#endif

#endif // __KSU_H_MANAGER_OBSERVER
