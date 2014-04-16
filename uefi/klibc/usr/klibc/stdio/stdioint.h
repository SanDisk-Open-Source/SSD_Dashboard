/*
 * stdioint.h
 *
 * stdio internals
 */

#ifndef USR_KLIBC_STDIO_STDIOINT_H
#define USR_KLIBC_STDIO_STDIOINT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

/* Actual FILE structure */
struct _IO_file_pvt {
	struct _IO_file pub;	/* Data exported to inlines */
	struct _IO_file_pvt *prev, *next;
	char *buf;		/* Buffer */
	char *data;		/* Location of input data in buffer */
	unsigned int ibytes;	/* Input data bytes in buffer */
	unsigned int obytes;	/* Output data bytes in buffer */
	unsigned int bufsiz;	/* Total size of buffer */
	enum _IO_bufmode bufmode; /* Type of buffering */
};

#define stdio_pvt(x) container_of(x, struct _IO_file_pvt, pub)

enum _IO_file_flags {
	_IO_FILE_FLAG_WRITE	=  1, /* Buffer has write data */
	_IO_FILE_FLAG_READ	=  2, /* Buffer has read data */
	_IO_FILE_FLAG_LINE_BUF  =  4, /* Line buffered */
	_IO_FILE_FLAG_UNBUF     =  8, /* Unbuffered */
	_IO_FILE_FLAG_EOF	= 16,
	_IO_FILE_FLAG_ERR	= 32,
};

/* Assign this much extra to the input buffer in case of ungetc() */
#define _IO_UNGET_SLOP	32

__extern int __fflush(struct _IO_file_pvt *);

__extern struct _IO_file_pvt __stdio_headnode;

#endif /* USR_KLIBC_STDIO_STDIOINT_H */
