/* go-map-len.c -- return the length of a map.

   Copyright 2009 The Go Authors. All rights reserved.
   Use of this source code is governed by a BSD-style
   license that can be found in the LICENSE file.  */

#include <stddef.h>

#include "map.h"

/* Return the length of a map.  This could be done inline, of course,
   but I'm doing it as a function for now to make it easy to chang the
   map structure.  */

size_t
__go_map_len (struct __go_map *map)
{
  if (map == NULL)
    return 0;
  return map->__element_count;
}
