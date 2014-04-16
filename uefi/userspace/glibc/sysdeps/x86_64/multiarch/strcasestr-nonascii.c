/* strstr with SSE4.2 intrinsics
   Copyright (C) 2010-2013 Free Software Foundation, Inc.
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

#include <ctype.h>
#include <xmmintrin.h>


/* Similar to __m128i_strloadu.  Convert to lower case for none-POSIX/C
   locale.  */
static __m128i
__m128i_strloadu_tolower (const unsigned char *p)
{
  union
    {
      char b[16];
      __m128i x;
    } u;

  for (int i = 0; i < 16; ++i)
    if (p[i] == 0)
      {
	u.b[i] = 0;
	break;
      }
    else
      u.b[i] = tolower (p[i]);

  return u.x;
}


#define STRCASESTR_NONASCII
#define USE_AS_STRCASESTR
#define STRSTR_SSE42 __strcasestr_sse42_nonascii
#include "strstr.c"
