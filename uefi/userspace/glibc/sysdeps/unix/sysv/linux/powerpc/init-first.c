/* Initialization code run first thing by the ELF startup code.  Linux/PowerPC.
   Copyright (C) 2007-2013 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#ifdef SHARED
# include <dl-vdso.h>
# undef __gettimeofday
# undef __clock_gettime
# undef __clock_getres
# include <bits/libc-vdso.h>

void *__vdso_gettimeofday attribute_hidden;
void *__vdso_clock_gettime;
void *__vdso_clock_getres;
void *__vdso_get_tbfreq;
void *__vdso_getcpu;
void *__vdso_time;

static inline void
_libc_vdso_platform_setup (void)
{
  PREPARE_VERSION (linux2615, "LINUX_2.6.15", 123718565);

  __vdso_gettimeofday = _dl_vdso_vsym ("__kernel_gettimeofday", &linux2615);

  __vdso_clock_gettime = _dl_vdso_vsym ("__kernel_clock_gettime", &linux2615);

  __vdso_clock_getres = _dl_vdso_vsym ("__kernel_clock_getres", &linux2615);

  __vdso_get_tbfreq = _dl_vdso_vsym ("__kernel_get_tbfreq", &linux2615);

  __vdso_getcpu = _dl_vdso_vsym ("__kernel_getcpu", &linux2615);

  __vdso_time = _dl_vdso_vsym ("__kernel_time", &linux2615);
}

# define VDSO_SETUP _libc_vdso_platform_setup
#endif

#include <csu/init-first.c>
