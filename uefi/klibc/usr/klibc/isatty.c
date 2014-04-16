/*
 * isatty.c
 */

#include <unistd.h>
#include <termios.h>
#include <errno.h>

int isatty(int fd)
{
	int dummy;

	/* All ttys support TIOCGPGRP */
	return !ioctl(fd, TIOCGPGRP, &dummy);
}
