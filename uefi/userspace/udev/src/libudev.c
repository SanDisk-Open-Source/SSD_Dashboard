/*
 * libudev - interface to udev device information
 *
 * Copyright (C) 2008-2010 Kay Sievers <kay.sievers@vrfy.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "libudev.h"
#include "libudev-private.h"

/**
 * SECTION:libudev
 * @short_description: libudev context
 *
 * The context contains the default values read from the udev config file,
 * and is passed to all library operations.
 */

/**
 * udev:
 *
 * Opaque object representing the library context.
 */
struct udev {
        int refcount;
        void (*log_fn)(struct udev *udev,
                       int priority, const char *file, int line, const char *fn,
                       const char *format, va_list args);
        void *userdata;
        char *sys_path;
        char *dev_path;
        char *rules_path[4];
        unsigned long long rules_path_ts[4];
        int rules_path_count;
        char *run_path;
        struct udev_list properties_list;
        int log_priority;
};

void udev_log(struct udev *udev,
              int priority, const char *file, int line, const char *fn,
              const char *format, ...)
{
        va_list args;

        va_start(args, format);
        udev->log_fn(udev, priority, file, line, fn, format, args);
        va_end(args);
}

static void log_stderr(struct udev *udev,
                       int priority, const char *file, int line, const char *fn,
                       const char *format, va_list args)
{
        fprintf(stderr, "libudev: %s: ", fn);
        vfprintf(stderr, format, args);
}

/**
 * udev_get_userdata:
 * @udev: udev library context
 *
 * Retrieve stored data pointer from library context. This might be useful
 * to access from callbacks like a custom logging function.
 *
 * Returns: stored userdata
 **/
UDEV_EXPORT void *udev_get_userdata(struct udev *udev)
{
        if (udev == NULL)
                return NULL;
        return udev->userdata;
}

/**
 * udev_set_userdata:
 * @udev: udev library context
 * @userdata: data pointer
 *
 * Store custom @userdata in the library context.
 **/
UDEV_EXPORT void udev_set_userdata(struct udev *udev, void *userdata)
{
        if (udev == NULL)
                return;
        udev->userdata = userdata;
}

static char *set_value(char **s, const char *v)
{
        free(*s);
        *s = strdup(v);
        util_remove_trailing_chars(*s, '/');
        return *s;
}

/**
 * udev_new:
 *
 * Create udev library context. This reads the udev configuration
 * file, and fills in the default values.
 *
 * The initial refcount is 1, and needs to be decremented to
 * release the resources of the udev library context.
 *
 * Returns: a new udev library context
 **/
UDEV_EXPORT struct udev *udev_new(void)
{
        struct udev *udev;
        const char *env;
        char *config_file = NULL;
        FILE *f;

        udev = calloc(1, sizeof(struct udev));
        if (udev == NULL)
                return NULL;
        udev->refcount = 1;
        udev->log_fn = log_stderr;
        udev->log_priority = LOG_ERR;
        udev_list_init(udev, &udev->properties_list, true);

        /* custom config file */
        env = getenv("UDEV_CONFIG_FILE");
        if (env != NULL) {
                if (set_value(&config_file, env) == NULL)
                        goto err;
                udev_add_property(udev, "UDEV_CONFIG_FILE", config_file);
        }

        /* default config file */
        if (config_file == NULL)
                config_file = strdup(SYSCONFDIR "/udev/udev.conf");
        if (config_file == NULL)
                goto err;

        f = fopen(config_file, "re");
        if (f != NULL) {
                char line[UTIL_LINE_SIZE];
                int line_nr = 0;

                while (fgets(line, sizeof(line), f)) {
                        size_t len;
                        char *key;
                        char *val;

                        line_nr++;

                        /* find key */
                        key = line;
                        while (isspace(key[0]))
                                key++;

                        /* comment or empty line */
                        if (key[0] == '#' || key[0] == '\0')
                                continue;

                        /* split key/value */
                        val = strchr(key, '=');
                        if (val == NULL) {
                                err(udev, "missing <key>=<value> in '%s'[%i], skip line\n", config_file, line_nr);
                                continue;
                        }
                        val[0] = '\0';
                        val++;

                        /* find value */
                        while (isspace(val[0]))
                                val++;

                        /* terminate key */
                        len = strlen(key);
                        if (len == 0)
                                continue;
                        while (isspace(key[len-1]))
                                len--;
                        key[len] = '\0';

                        /* terminate value */
                        len = strlen(val);
                        if (len == 0)
                                continue;
                        while (isspace(val[len-1]))
                                len--;
                        val[len] = '\0';

                        if (len == 0)
                                continue;

                        /* unquote */
                        if (val[0] == '"' || val[0] == '\'') {
                                if (val[len-1] != val[0]) {
                                        err(udev, "inconsistent quoting in '%s'[%i], skip line\n", config_file, line_nr);
                                        continue;
                                }
                                val[len-1] = '\0';
                                val++;
                        }

                        if (strcmp(key, "udev_log") == 0) {
                                udev_set_log_priority(udev, util_log_priority(val));
                                continue;
                        }
                        if (strcmp(key, "udev_root") == 0) {
                                set_value(&udev->dev_path, val);
                                continue;
                        }
                        if (strcmp(key, "udev_run") == 0) {
                                set_value(&udev->run_path, val);
                                continue;
                        }
                        if (strcmp(key, "udev_sys") == 0) {
                                set_value(&udev->sys_path, val);
                                continue;
                        }
                        if (strcmp(key, "udev_rules") == 0) {
                                set_value(&udev->rules_path[0], val);
                                udev->rules_path_count = 1;
                                continue;
                        }
                }
                fclose(f);
        }

        /* environment overrides config */
        env = getenv("UDEV_LOG");
        if (env != NULL)
                udev_set_log_priority(udev, util_log_priority(env));

        /* set defaults */
        if (udev->dev_path == NULL)
                if (set_value(&udev->dev_path, "/dev") == NULL)
                        goto err;

        if (udev->sys_path == NULL)
                if (set_value(&udev->sys_path, "/sys") == NULL)
                        goto err;

        if (udev->run_path == NULL)
                if (set_value(&udev->run_path, "/run/udev") == NULL)
                        goto err;

        if (udev->rules_path[0] == NULL) {
                /* /usr/lib/udev -- system rules */
                udev->rules_path[0] = strdup(PKGLIBEXECDIR "/rules.d");
                if (!udev->rules_path[0])
                        goto err;

                /* /run/udev -- runtime rules */
                if (asprintf(&udev->rules_path[2], "%s/rules.d", udev->run_path) < 0)
                        goto err;

                /* /etc/udev -- local administration rules */
                udev->rules_path[1] = strdup(SYSCONFDIR "/udev/rules.d");
                if (!udev->rules_path[1])
                        goto err;

                udev->rules_path_count = 3;
        }

        dbg(udev, "context %p created\n", udev);
        dbg(udev, "log_priority=%d\n", udev->log_priority);
        dbg(udev, "config_file='%s'\n", config_file);
        dbg(udev, "dev_path='%s'\n", udev->dev_path);
        dbg(udev, "sys_path='%s'\n", udev->sys_path);
        dbg(udev, "run_path='%s'\n", udev->run_path);
        dbg(udev, "rules_path='%s':'%s':'%s'\n", udev->rules_path[0], udev->rules_path[1], udev->rules_path[2]);
        free(config_file);
        return udev;
err:
        free(config_file);
        err(udev, "context creation failed\n");
        udev_unref(udev);
        return NULL;
}

/**
 * udev_ref:
 * @udev: udev library context
 *
 * Take a reference of the udev library context.
 *
 * Returns: the passed udev library context
 **/
UDEV_EXPORT struct udev *udev_ref(struct udev *udev)
{
        if (udev == NULL)
                return NULL;
        udev->refcount++;
        return udev;
}

/**
 * udev_unref:
 * @udev: udev library context
 *
 * Drop a reference of the udev library context. If the refcount
 * reaches zero, the resources of the context will be released.
 *
 **/
UDEV_EXPORT void udev_unref(struct udev *udev)
{
        if (udev == NULL)
                return;
        udev->refcount--;
        if (udev->refcount > 0)
                return;
        udev_list_cleanup(&udev->properties_list);
        free(udev->dev_path);
        free(udev->sys_path);
        free(udev->rules_path[0]);
        free(udev->rules_path[1]);
        free(udev->rules_path[2]);
        free(udev->run_path);
        dbg(udev, "context %p released\n", udev);
        free(udev);
}

/**
 * udev_set_log_fn:
 * @udev: udev library context
 * @log_fn: function to be called for logging messages
 *
 * The built-in logging writes to stderr. It can be
 * overridden by a custom function, to plug log messages
 * into the users' logging functionality.
 *
 **/
UDEV_EXPORT void udev_set_log_fn(struct udev *udev,
                     void (*log_fn)(struct udev *udev,
                                    int priority, const char *file, int line, const char *fn,
                                    const char *format, va_list args))
{
        udev->log_fn = log_fn;
        info(udev, "custom logging function %p registered\n", log_fn);
}

/**
 * udev_get_log_priority:
 * @udev: udev library context
 *
 * The initial logging priority is read from the udev config file
 * at startup.
 *
 * Returns: the current logging priority
 **/
UDEV_EXPORT int udev_get_log_priority(struct udev *udev)
{
        return udev->log_priority;
}

/**
 * udev_set_log_priority:
 * @udev: udev library context
 * @priority: the new logging priority
 *
 * Set the current logging priority. The value controls which messages
 * are logged.
 **/
UDEV_EXPORT void udev_set_log_priority(struct udev *udev, int priority)
{
        char num[32];

        udev->log_priority = priority;
        snprintf(num, sizeof(num), "%u", udev->log_priority);
        udev_add_property(udev, "UDEV_LOG", num);
}

int udev_get_rules_path(struct udev *udev, char **path[], unsigned long long *stamp_usec[])
{
        *path = udev->rules_path;
        if (stamp_usec)
                *stamp_usec = udev->rules_path_ts;
        return udev->rules_path_count;
}

/**
 * udev_get_sys_path:
 * @udev: udev library context
 *
 * Retrieve the sysfs mount point. The default is "/sys". For
 * testing purposes, it can be overridden with udev_sys=
 * in the udev configuration file.
 *
 * Returns: the sys mount point
 **/
UDEV_EXPORT const char *udev_get_sys_path(struct udev *udev)
{
        if (udev == NULL)
                return NULL;
        return udev->sys_path;
}

/**
 * udev_get_dev_path:
 * @udev: udev library context
 *
 * Retrieve the device directory path. The default value is "/dev",
 * the actual value may be overridden in the udev configuration
 * file.
 *
 * Returns: the device directory path
 **/
UDEV_EXPORT const char *udev_get_dev_path(struct udev *udev)
{
        if (udev == NULL)
                return NULL;
        return udev->dev_path;
}

/**
 * udev_get_run_path:
 * @udev: udev library context
 *
 * Retrieve the udev runtime directory path. The default is "/run/udev".
 *
 * Returns: the runtime directory path
 **/
UDEV_EXPORT const char *udev_get_run_path(struct udev *udev)
{
        if (udev == NULL)
                return NULL;
        return udev->run_path;
}

struct udev_list_entry *udev_add_property(struct udev *udev, const char *key, const char *value)
{
        if (value == NULL) {
                struct udev_list_entry *list_entry;

                list_entry = udev_get_properties_list_entry(udev);
                list_entry = udev_list_entry_get_by_name(list_entry, key);
                if (list_entry != NULL)
                        udev_list_entry_delete(list_entry);
                return NULL;
        }
        return udev_list_entry_add(&udev->properties_list, key, value);
}

struct udev_list_entry *udev_get_properties_list_entry(struct udev *udev)
{
        return udev_list_get_entry(&udev->properties_list);
}
