/*
 * bsd_signal.c
 */

#include <signal.h>

__sighandler_t bsd_signal(int signum, __sighandler_t handler)
{
	/* BSD signal() semantics */
	return __signal(signum, handler, SA_RESTART);
}

__sighandler_t signal(int signum, __sighandler_t handler)
  __alias("bsd_signal");
