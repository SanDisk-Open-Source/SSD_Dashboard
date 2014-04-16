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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#include "testsuite.h"

static __noreturn int modinfo_jonsmodules(const struct test *t)
{
	const char *progname = ABS_TOP_BUILDDIR "/tools/modinfo";
	const char *const args[] = {
		progname,
		"/ext4-i686.ko", "/ext4-ppc64.ko", "/ext4-s390x.ko",
		"/ext4-x86_64.ko",
		NULL,
	};

	test_spawn_prog(progname, args);
	exit(EXIT_FAILURE);
}
static DEFINE_TEST(modinfo_jonsmodules,
	.description = "check if output for modinfo is correct for i686, ppc64, s390x and x86_64",
	.config = {
		[TC_ROOTFS] = TESTSUITE_ROOTFS "test-modinfo/",
	},
	.output = {
		.stdout = TESTSUITE_ROOTFS "test-modinfo/correct.txt",
	});

static const struct test *tests[] = {
	&smodinfo_jonsmodules,
	NULL,
};

TESTSUITE_MAIN(tests);
