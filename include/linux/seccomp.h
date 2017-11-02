#ifndef _LINUX_SECCOMP_H
#define _LINUX_SECCOMP_H


#ifdef CONFIG_SECCOMP

#include <linux/thread_info.h>
#include <asm/seccomp.h>

/* Valid operations for seccomp syscall. */
#define SECCOMP_SET_MODE_STRICT	0
#define SECCOMP_SET_MODE_FILTER	1

typedef struct { int mode; } seccomp_t;

extern int __secure_computing(int);
static inline void secure_computing(int this_syscall)
{
	if (unlikely(test_thread_flag(TIF_SECCOMP)))
		__secure_computing(this_syscall);
}

extern long prctl_get_seccomp(void);
extern long prctl_set_seccomp(unsigned long, char __user *);

#else /* CONFIG_SECCOMP */

#include <linux/errno.h>

typedef struct { } seccomp_t;

#define secure_computing(x) do { } while (0)

static inline long prctl_get_seccomp(void)
{
	return -EINVAL;
}

static inline long prctl_set_seccomp(unsigned long arg2)
{
	return -EINVAL;
}

#endif /* CONFIG_SECCOMP */

#endif /* _LINUX_SECCOMP_H */
