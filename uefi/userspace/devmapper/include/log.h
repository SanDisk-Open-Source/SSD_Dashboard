/*
 * Copyright (C) 2001-2004 Sistina Software, Inc. All rights reserved.
 * Copyright (C) 2004-2007 Red Hat, Inc. All rights reserved.
 *
 * This file is part of the device-mapper userspace tools.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU Lesser General Public License v.2.1.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _DM_LOG_H
#define _DM_LOG_H

#include <stdio.h>		/* FILE */
#include <string.h>		/* strerror() */
#include <errno.h>

#define _LOG_STDERR 128 /* force things to go to stderr, even if loglevel
			   would make them go to stdout */
#define _LOG_DEBUG 7
#define _LOG_INFO 6
#define _LOG_NOTICE 5
#define _LOG_WARN 4
#define _LOG_ERR 3
#define _LOG_FATAL 2

#define log_debug(x...) plog(_LOG_DEBUG, x)
#define log_info(x...) plog(_LOG_INFO, x)
#define log_notice(x...) plog(_LOG_NOTICE, x)
#define log_warn(x...) plog(_LOG_WARN | _LOG_STDERR, x)
#define log_err(x...) plog(_LOG_ERR, x)
#define log_fatal(x...) plog(_LOG_FATAL, x)

#define stack log_debug("<backtrace>")	/* Backtrace on error */
#define log_very_verbose(args...) log_info(args)
#define log_verbose(args...) log_notice(args)
#define log_print(args...) plog(_LOG_WARN, args)
#define log_error(args...) log_err(args)

/* System call equivalents */
#define log_sys_error(x, y) \
		log_err("%s: %s failed: %s", y, x, strerror(errno))
#define log_sys_very_verbose(x, y) \
		log_info("%s: %s failed: %s", y, x, strerror(errno))
#define log_sys_debug(x, y) \
		log_debug("%s: %s failed: %s", y, x, strerror(errno))

#define return_0	do { stack; return 0; } while (0)
#define return_NULL	do { stack; return NULL; } while (0)
#define goto_out	do { stack; goto out; } while (0)
#define goto_bad	do { stack; goto bad; } while (0)

#endif
