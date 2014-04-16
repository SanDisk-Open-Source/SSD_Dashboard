/*
  unparse_path.[ch]
 
  Copyright (C) 2001 Dell Computer Corporation <Matt_Domsch@dell.com>
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _UNPARSE_PATH_H
#define _UNPARSE_PATH_H

#include <stdint.h>
#include "efi.h"

#define OFFSET_OF(struct_type, member)    \
    ((unsigned long) ((char *) &((struct_type*) 0)->member))

uint64_t unparse_path(char *buffer, EFI_DEVICE_PATH *path, uint16_t pathsize);
void dump_raw_data(void *data, uint64_t length);
unsigned long unparse_raw(char *buffer, uint8_t *p, uint64_t length);
unsigned long unparse_raw_text(char *buffer, uint8_t *p, uint64_t length);

#endif
