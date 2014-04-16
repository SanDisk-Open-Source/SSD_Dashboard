#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <locale.h>

#include <attr/error_context.h>
#include <attr/libattr.h>
#include <acl/libacl.h>

void
error(struct error_context *ctx, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (vfprintf(stderr, fmt, ap))
		fprintf(stderr, ": ");
	fprintf(stderr, "%s\n", strerror(errno));
	va_end(ap);
}

struct error_context ctx = {
	error
};

int
main(int argc, char *argv[])
{
	int ret;

	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");

	if (argc != 3) {
		fprintf(stderr, "Usage: %s from to\n", argv[0]);
		exit(1);
	}

	ret = perm_copy_file(argv[1], argv[2], &ctx);
	exit (ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

