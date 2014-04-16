/* go-map-delete.c -- delete an entry from a map.

   Copyright 2009 The Go Authors. All rights reserved.
   Use of this source code is governed by a BSD-style
   license that can be found in the LICENSE file.  */

#include <stddef.h>
#include <stdlib.h>

#include "go-alloc.h"
#include "go-assert.h"
#include "map.h"

/* Delete the entry matching KEY from MAP.  */

void
__go_map_delete (struct __go_map *map, const void *key)
{
  const struct __go_map_descriptor *descriptor;
  const struct __go_type_descriptor *key_descriptor;
  size_t key_offset;
  _Bool (*equalfn) (const void*, const void*, size_t);
  size_t key_hash;
  size_t key_size;
  size_t bucket_index;
  void **pentry;

  descriptor = map->__descriptor;

  key_descriptor = descriptor->__map_descriptor->__key_type;
  key_offset = descriptor->__key_offset;
  key_size = key_descriptor->__size;
  __go_assert (key_size != 0 && key_size != -1UL);
  equalfn = key_descriptor->__equalfn;

  key_hash = key_descriptor->__hashfn (key, key_size);
  bucket_index = key_hash % map->__bucket_count;

  pentry = map->__buckets + bucket_index;
  while (*pentry != NULL)
    {
      char *entry = (char *) *pentry;
      if (equalfn (key, entry + key_offset, key_size))
	{
	  *pentry = *(void **) entry;
	  __go_free (entry);
	  map->__element_count -= 1;
	  break;
	}
      pentry = (void **) entry;
    }
}
