#ifndef _LIBKMOD_UTIL_H_
#define _LIBKMOD_UTIL_H_

#include "macro.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>


char *getline_wrapped(FILE *fp, unsigned int *linenum) __attribute__((nonnull(1)));
#define streq(a, b) (strcmp((a), (b)) == 0)
#define strstartswith(a, b) (strncmp(a, b, strlen(b)) == 0)
void *memdup(const void *p, size_t n) __attribute__((nonnull(1)));

ssize_t read_str_safe(int fd, char *buf, size_t buflen) _must_check_ __attribute__((nonnull(2)));
ssize_t write_str_safe(int fd, const char *buf, size_t buflen) __attribute__((nonnull(2)));
int read_str_long(int fd, long *value, int base) _must_check_ __attribute__((nonnull(2)));
int read_str_ulong(int fd, unsigned long *value, int base) _must_check_ __attribute__((nonnull(2)));
char *strchr_replace(char *s, int c, char r);
bool path_is_absolute(const char *p) _must_check_ __attribute__((nonnull(1)));
char *path_make_absolute_cwd(const char *p) _must_check_ __attribute__((nonnull(1)));
int alias_normalize(const char *alias, char buf[PATH_MAX], size_t *len) _must_check_ __attribute__((nonnull(1,2)));
char *modname_normalize(const char *modname, char buf[PATH_MAX], size_t *len) __attribute__((nonnull(1, 2)));
char *path_to_modname(const char *path, char buf[PATH_MAX], size_t *len) __attribute__((nonnull(2)));
unsigned long long stat_mstamp(const struct stat *st);
unsigned long long ts_usec(const struct timespec *ts);

#define get_unaligned(ptr)			\
({						\
	struct __attribute__((packed)) {	\
		typeof(*(ptr)) __v;		\
	} *__p = (typeof(__p)) (ptr);		\
	__p->__v;				\
})

#define put_unaligned(val, ptr)		\
do {						\
	struct __attribute__((packed)) {	\
		typeof(*(ptr)) __v;		\
	} *__p = (typeof(__p)) (ptr);		\
	__p->__v = (val);			\
} while(0)

#endif
