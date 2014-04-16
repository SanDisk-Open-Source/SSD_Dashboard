/*
 * include/arch/sparc/klibc/archconfig.h
 *
 * See include/klibc/sysconfig.h for the options that can be set in
 * this file.
 *
 */

#ifndef _KLIBC_ARCHCONFIG_H
#define _KLIBC_ARCHCONFIG_H

#define _KLIBC_USE_RT_SIG 1	/* Use rt_* signals */
#define _KLIBC_SYS_SOCKETCALL 1 /* Use sys_socketcall unconditionally */

#endif				/* _KLIBC_ARCHCONFIG_H */
