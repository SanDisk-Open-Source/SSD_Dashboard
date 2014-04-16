/* go-int-to-string.c -- convert an integer to a string in Go.

   Copyright 2009 The Go Authors. All rights reserved.
   Use of this source code is governed by a BSD-style
   license that can be found in the LICENSE file.  */

#include "go-string.h"
#include "runtime.h"
#include "malloc.h"

struct __go_string
__go_int_to_string (int v)
{
  char buf[4];
  int len;
  unsigned char *retdata;
  struct __go_string ret;

  if (v <= 0x7f)
    {
      buf[0] = v;
      len = 1;
    }
  else if (v <= 0x7ff)
    {
      buf[0] = 0xc0 + (v >> 6);
      buf[1] = 0x80 + (v & 0x3f);
      len = 2;
    }
  else
    {
      /* If the value is out of range for UTF-8, turn it into the
	 "replacement character".  */
      if (v > 0x10ffff)
	v = 0xfffd;

      if (v <= 0xffff)
	{
	  buf[0] = 0xe0 + (v >> 12);
	  buf[1] = 0x80 + ((v >> 6) & 0x3f);
	  buf[2] = 0x80 + (v & 0x3f);
	  len = 3;
	}
      else
	{
	  buf[0] = 0xf0 + (v >> 18);
	  buf[1] = 0x80 + ((v >> 12) & 0x3f);
	  buf[2] = 0x80 + ((v >> 6) & 0x3f);
	  buf[3] = 0x80 + (v & 0x3f);
	  len = 4;
	}
    }

  retdata = runtime_mallocgc (len, RefNoPointers, 1, 0);
  __builtin_memcpy (retdata, buf, len);
  ret.__data = retdata;
  ret.__length = len;

  return ret;
}
