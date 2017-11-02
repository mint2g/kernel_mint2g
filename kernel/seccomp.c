/*
 * linux/kernel/seccomp.c
 *
 * Copyright 2004-2005  Andrea Arcangeli <andrea@cpushare.com>
 *
 * This defines a simple but solid secure-computing mode.
 */

#include <linux/seccomp.h>
#include <linux/sched.h>
#include <linux/compat.h>
#include <linux/syscalls.h>

/* #define SECCOMP_DEBUG 1 */
#define NR_SECCOMP_MODES 1

/*
 * Secure computing mode 1 allows only read/write/exit/sigreturn.
 * To be fully secure this must be combined with rlimit
 * to limit the stack allocations too.
 */
static int mode1_syscalls[] = {
	__NR_seccomp_read, __NR_seccomp_write, __NR_seccomp_exit, __NR_seccomp_sigreturn,
	0, /* null terminated */
};

#ifdef CONFIG_COMPAT
static int mode1_syscalls_32[] = {
	__NR_seccomp_read_32, __NR_seccomp_write_32, __NR_seccomp_exit_32, __NR_seccomp_sigreturn_32,
	0, /* null terminated */
};
#endif

int __secure_computing(int this_syscall)
{
int exit_sig = 0;
return  exit_sig;
}

long prctl_get_seccomp(void)
{
	return current->seccomp.mode;
}

long prctl_set_seccomp(unsigned long seccomp_mode, char __user *filter)
{
return 0;
}

SYSCALL_DEFINE3(seccomp, unsigned int, op, unsigned int, flags,
			 const char __user *, uargs)
{
	return 0;
}
