/*
 * dirent.h
 */

#ifndef _DIRENT_H
#define _DIRENT_H

#include <klibc/compiler.h>
#include <klibc/extern.h>
#include <klibc/sysconfig.h>
#include <sys/dirent.h>

struct _IO_dir {
	int __fd;

#ifdef __KLIBC_DIRENT_INTERNALS
	/* These fields for internal use only */

	size_t bytes_left;
	struct dirent *next;
	/* Declaring this as an array of struct enforces correct alignment */
	struct dirent buffer[_KLIBC_BUFSIZ / sizeof(struct dirent)];
#endif
};
typedef struct _IO_dir DIR;

__extern DIR *fdopendir(int);
__extern DIR *opendir(const char *);
__extern struct dirent *readdir(DIR *);
__extern int closedir(DIR *);
__static_inline int dirfd(DIR * __d)
{
	return __d->__fd;
}

__extern int scandir(const char *, struct dirent ***,
		     int (*)(const struct dirent *),
		     int (*)(const struct dirent **,
			     const struct dirent **));

__extern int alphasort(const struct dirent **, const struct dirent **);

#endif				/* _DIRENT_H */
