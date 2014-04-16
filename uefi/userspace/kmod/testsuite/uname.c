/*
 * Copyright (C) 2012  ProFUSION embedded systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <dlfcn.h>
#include <sys/utsname.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "testsuite.h"

TS_EXPORT int uname(struct utsname *u)
{
	static void *nextlib = NULL;
	static int (*nextlib_uname)(struct utsname *u);
	const char *release = getenv(S_TC_UNAME_R);
	int err;
	size_t sz;

	if (release == NULL) {
		fprintf(stderr, "TRAP uname(): missing export %s?\n",
							S_TC_UNAME_R);
		errno = EFAULT;
		return -1;
	}

	if (nextlib == NULL) {
#ifdef RTLD_NEXT
		nextlib = RTLD_NEXT;
#else
		nextlib = dlopen("libc.so.6", RTLD_LAZY);
#endif
		nextlib_uname = dlsym(nextlib, "uname");
	}

	err = nextlib_uname(u);
	if (err < 0)
		return err;

	sz = strlen(release) + 1;
	if (sz > sizeof(u->release)) {
		fprintf(stderr, "uname(): sizeof release (%s) "
				"is greater than available space: %zu",
				release, sizeof(u->release));
		errno = -EFAULT;
		return -1;
	}

	memcpy(u->release, release, sz);
	return 0;
}
