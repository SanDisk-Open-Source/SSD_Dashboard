/* Copyright (C) 1991-2013 Free Software Foundation, Inc.
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

#include <errno.h>
#include <stddef.h>
#include <signal.h>


/* If SET is not NULL, modify the current set of blocked signals
   according to HOW, which may be SIG_BLOCK, SIG_UNBLOCK or SIG_SETMASK.
   If OSET is not NULL, store the old set of blocked signals in *OSET.  */
int
__sigprocmask (how, set, oset)
     int how;
     const sigset_t *set;
     sigset_t *oset;
{
  int mask;

  if (set != NULL)
    {
      mask = *set;
      switch (how)
	{
	case SIG_BLOCK:
	  mask = __sigblock (mask);
	  break;

	case SIG_UNBLOCK:
	  mask = __sigblock (0) & ~mask;
	  /* Fall through.  */

	case SIG_SETMASK:
	  mask = __sigsetmask (mask);
	  break;

	default:
	  __set_errno (EINVAL);
	  return -1;
	}
    }
  else
    mask = __sigblock (0);

  if (oset != NULL)
    *oset = mask;

  return 0;
}

weak_alias (__sigprocmask, sigprocmask)
