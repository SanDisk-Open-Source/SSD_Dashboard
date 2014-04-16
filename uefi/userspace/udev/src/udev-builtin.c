/*
 * Copyright (C) 2007-2009 Kay Sievers <kay.sievers@vrfy.org>
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "udev.h"

static const struct udev_builtin *builtins[] = {
        [UDEV_BUILTIN_BLKID] = &udev_builtin_blkid,
        [UDEV_BUILTIN_FIRMWARE] = &udev_builtin_firmware,
        [UDEV_BUILTIN_INPUT_ID] = &udev_builtin_input_id,
        [UDEV_BUILTIN_KMOD] = &udev_builtin_kmod,
        [UDEV_BUILTIN_PATH_ID] = &udev_builtin_path_id,
        [UDEV_BUILTIN_PCI_DB] = &udev_builtin_pci_db,
        [UDEV_BUILTIN_USB_DB] = &udev_builtin_usb_db,
        [UDEV_BUILTIN_USB_ID] = &udev_builtin_usb_id,
};

int udev_builtin_init(struct udev *udev)
{
        unsigned int i;
        int err;

        for (i = 0; i < ARRAY_SIZE(builtins); i++) {
                if (builtins[i]->init) {
                        err = builtins[i]->init(udev);
                        if (err < 0)
                                break;
                }
        }
        return err;
}

void udev_builtin_exit(struct udev *udev)
{
        unsigned int i;

        for (i = 0; i < ARRAY_SIZE(builtins); i++)
                if (builtins[i]->exit)
                        builtins[i]->exit(udev);
}

bool udev_builtin_validate(struct udev *udev)
{
        unsigned int i;
        bool change = false;

        for (i = 0; i < ARRAY_SIZE(builtins); i++)
                if (builtins[i]->validate)
                        if (builtins[i]->validate(udev))
                                change = true;
        return change;
}

void udev_builtin_list(struct udev *udev)
{
        unsigned int i;

        for (i = 0; i < ARRAY_SIZE(builtins); i++)
                fprintf(stderr, "  %-12s %s\n", builtins[i]->name, builtins[i]->help);
}

const char *udev_builtin_name(enum udev_builtin_cmd cmd)
{
        return builtins[cmd]->name;
}

bool udev_builtin_run_once(enum udev_builtin_cmd cmd)
{
        return builtins[cmd]->run_once;
}

enum udev_builtin_cmd udev_builtin_lookup(const char *command)
{
        char name[UTIL_PATH_SIZE];
        enum udev_builtin_cmd i;
        char *pos;

        util_strscpy(name, sizeof(name), command);
        pos = strchr(name, ' ');
        if (pos)
                pos[0] = '\0';
        for (i = 0; i < ARRAY_SIZE(builtins); i++)
                if (strcmp(builtins[i]->name, name) == 0)
                        return i;
        return UDEV_BUILTIN_MAX;
}

int udev_builtin_run(struct udev_device *dev, enum udev_builtin_cmd cmd, const char *command, bool test)
{
        char arg[UTIL_PATH_SIZE];
        int argc;
        char *argv[128];

        optind = 0;
        util_strscpy(arg, sizeof(arg), command);
        udev_build_argv(udev_device_get_udev(dev), arg, &argc, argv);
        return builtins[cmd]->cmd(dev, argc, argv, test);
}

int udev_builtin_add_property(struct udev_device *dev, bool test, const char *key, const char *val)
{
        struct udev_list_entry *entry;

        entry = udev_device_add_property(dev, key, val);
        /* store in db, skip private keys */
        if (key[0] != '.')
                udev_list_entry_set_num(entry, true);

        info(udev_device_get_udev(dev), "%s=%s\n", key, val);
        if (test)
                printf("%s=%s\n", key, val);
        return 0;
}
