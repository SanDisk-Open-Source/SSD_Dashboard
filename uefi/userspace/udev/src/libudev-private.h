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

#ifndef _LIBUDEV_PRIVATE_H_
#define _LIBUDEV_PRIVATE_H_

#include <syslog.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include "libudev.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define READ_END                                0
#define WRITE_END                                1

static inline void __attribute__((always_inline, format(printf, 2, 3)))
udev_log_null(struct udev *udev, const char *format, ...) {}

#define udev_log_cond(udev, prio, arg...) \
  do { \
    if (udev_get_log_priority(udev) >= prio) \
      udev_log(udev, prio, __FILE__, __LINE__, __FUNCTION__, ## arg); \
  } while (0)

#ifdef ENABLE_LOGGING
#  ifdef ENABLE_DEBUG
#    define dbg(udev, arg...) udev_log_cond(udev, LOG_DEBUG, ## arg)
#  else
#    define dbg(udev, arg...) udev_log_null(udev, ## arg)
#  endif
#  define info(udev, arg...) udev_log_cond(udev, LOG_INFO, ## arg)
#  define err(udev, arg...) udev_log_cond(udev, LOG_ERR, ## arg)
#else
#  define dbg(udev, arg...) udev_log_null(udev, ## arg)
#  define info(udev, arg...) udev_log_null(udev, ## arg)
#  define err(udev, arg...) udev_log_null(udev, ## arg)
#endif

#define UDEV_EXPORT __attribute__ ((visibility("default")))

static inline void udev_log_init(const char *program_name)
{
        openlog(program_name, LOG_PID | LOG_CONS, LOG_DAEMON);
}

static inline void udev_log_close(void)
{
        closelog();
}

/* libudev.c */
void udev_log(struct udev *udev,
              int priority, const char *file, int line, const char *fn,
              const char *format, ...)
              __attribute__((format(printf, 6, 7)));
int udev_get_rules_path(struct udev *udev, char **path[], unsigned long long *ts_usec[]);
struct udev_list_entry *udev_add_property(struct udev *udev, const char *key, const char *value);
struct udev_list_entry *udev_get_properties_list_entry(struct udev *udev);

/* libudev-device.c */
struct udev_device *udev_device_new(struct udev *udev);
struct udev_device *udev_device_new_from_id_filename(struct udev *udev, char *id);
mode_t udev_device_get_devnode_mode(struct udev_device *udev_device);
int udev_device_set_syspath(struct udev_device *udev_device, const char *syspath);
int udev_device_set_devnode(struct udev_device *udev_device, const char *devnode);
int udev_device_add_devlink(struct udev_device *udev_device, const char *devlink, int unique);
void udev_device_cleanup_devlinks_list(struct udev_device *udev_device);
struct udev_list_entry *udev_device_add_property(struct udev_device *udev_device, const char *key, const char *value);
void udev_device_add_property_from_string_parse(struct udev_device *udev_device, const char *property);
int udev_device_add_property_from_string_parse_finish(struct udev_device *udev_device);
char **udev_device_get_properties_envp(struct udev_device *udev_device);
ssize_t udev_device_get_properties_monitor_buf(struct udev_device *udev_device, const char **buf);
int udev_device_read_db(struct udev_device *udev_device, const char *dbfile);
int udev_device_read_uevent_file(struct udev_device *udev_device);
int udev_device_set_action(struct udev_device *udev_device, const char *action);
const char *udev_device_get_devpath_old(struct udev_device *udev_device);
const char *udev_device_get_id_filename(struct udev_device *udev_device);
void udev_device_set_is_initialized(struct udev_device *udev_device);
int udev_device_add_tag(struct udev_device *udev_device, const char *tag);
void udev_device_cleanup_tags_list(struct udev_device *udev_device);
unsigned long long udev_device_get_usec_initialized(struct udev_device *udev_device);
void udev_device_set_usec_initialized(struct udev_device *udev_device, unsigned long long usec_initialized);
int udev_device_get_devlink_priority(struct udev_device *udev_device);
int udev_device_set_devlink_priority(struct udev_device *udev_device, int prio);
int udev_device_get_watch_handle(struct udev_device *udev_device);
int udev_device_set_watch_handle(struct udev_device *udev_device, int handle);
int udev_device_get_ifindex(struct udev_device *udev_device);
void udev_device_set_info_loaded(struct udev_device *device);
bool udev_device_get_db_persist(struct udev_device *udev_device);
void udev_device_set_db_persist(struct udev_device *udev_device);

/* libudev-device-private.c */
int udev_device_update_db(struct udev_device *udev_device);
int udev_device_delete_db(struct udev_device *udev_device);
int udev_device_tag_index(struct udev_device *dev, struct udev_device *dev_old, bool add);

/* libudev-monitor.c - netlink/unix socket communication  */
int udev_monitor_disconnect(struct udev_monitor *udev_monitor);
int udev_monitor_allow_unicast_sender(struct udev_monitor *udev_monitor, struct udev_monitor *sender);
int udev_monitor_send_device(struct udev_monitor *udev_monitor,
                             struct udev_monitor *destination, struct udev_device *udev_device);
struct udev_monitor *udev_monitor_new_from_netlink_fd(struct udev *udev, const char *name, int fd);

/* libudev-list.c */
struct udev_list_node {
        struct udev_list_node *next, *prev;
};
struct udev_list {
        struct udev *udev;
        struct udev_list_node node;
        struct udev_list_entry **entries;
        unsigned int entries_cur;
        unsigned int entries_max;
        bool unique;
};
#define UDEV_LIST(list) struct udev_list_node list = { &(list), &(list) }
void udev_list_node_init(struct udev_list_node *list);
int udev_list_node_is_empty(struct udev_list_node *list);
void udev_list_node_append(struct udev_list_node *new, struct udev_list_node *list);
void udev_list_node_remove(struct udev_list_node *entry);
#define udev_list_node_foreach(node, list) \
        for (node = (list)->next; \
             node != list; \
             node = (node)->next)
#define udev_list_node_foreach_safe(node, tmp, list) \
        for (node = (list)->next, tmp = (node)->next; \
             node != list; \
             node = tmp, tmp = (tmp)->next)
void udev_list_init(struct udev *udev, struct udev_list *list, bool unique);
void udev_list_cleanup(struct udev_list *list);
struct udev_list_entry *udev_list_get_entry(struct udev_list *list);
struct udev_list_entry *udev_list_entry_add(struct udev_list *list, const char *name, const char *value);
void udev_list_entry_delete(struct udev_list_entry *entry);
void udev_list_entry_insert_before(struct udev_list_entry *new, struct udev_list_entry *entry);
void udev_list_entry_append(struct udev_list_entry *new, struct udev_list *list);
int udev_list_entry_get_num(struct udev_list_entry *list_entry);
void udev_list_entry_set_num(struct udev_list_entry *list_entry, int num);
#define udev_list_entry_foreach_safe(entry, tmp, first) \
        for (entry = first, tmp = udev_list_entry_get_next(entry); \
             entry != NULL; \
             entry = tmp, tmp = udev_list_entry_get_next(tmp))

/* libudev-queue.c */
unsigned long long int udev_get_kernel_seqnum(struct udev *udev);
int udev_queue_read_seqnum(FILE *queue_file, unsigned long long int *seqnum);
ssize_t udev_queue_read_devpath(FILE *queue_file, char *devpath, size_t size);
ssize_t udev_queue_skip_devpath(FILE *queue_file);

/* libudev-queue-private.c */
struct udev_queue_export *udev_queue_export_new(struct udev *udev);
struct udev_queue_export *udev_queue_export_unref(struct udev_queue_export *udev_queue_export);
void udev_queue_export_cleanup(struct udev_queue_export *udev_queue_export);
int udev_queue_export_device_queued(struct udev_queue_export *udev_queue_export, struct udev_device *udev_device);
int udev_queue_export_device_finished(struct udev_queue_export *udev_queue_export, struct udev_device *udev_device);

/* libudev-util.c */
#define UTIL_PATH_SIZE                                1024
#define UTIL_NAME_SIZE                                512
#define UTIL_LINE_SIZE                                16384
#define UDEV_ALLOWED_CHARS_INPUT                "/ $%?,"
ssize_t util_get_sys_core_link_value(struct udev *udev, const char *slink, const char *syspath, char *value, size_t size);
int util_resolve_sys_link(struct udev *udev, char *syspath, size_t size);
int util_log_priority(const char *priority);
size_t util_path_encode(const char *src, char *dest, size_t size);
size_t util_path_decode(char *s);
void util_remove_trailing_chars(char *path, char c);
size_t util_strpcpy(char **dest, size_t size, const char *src);
size_t util_strpcpyl(char **dest, size_t size, const char *src, ...) __attribute__((sentinel));
size_t util_strscpy(char *dest, size_t size, const char *src);
size_t util_strscpyl(char *dest, size_t size, const char *src, ...) __attribute__((sentinel));
int util_replace_whitespace(const char *str, char *to, size_t len);
int util_replace_chars(char *str, const char *white);
unsigned int util_string_hash32(const char *key);
uint64_t util_string_bloom64(const char *str);

/* libudev-util-private.c */
int util_create_path(struct udev *udev, const char *path);
int util_create_path_selinux(struct udev *udev, const char *path);
int util_delete_path(struct udev *udev, const char *path);
uid_t util_lookup_user(struct udev *udev, const char *user);
gid_t util_lookup_group(struct udev *udev, const char *group);
int util_resolve_subsys_kernel(struct udev *udev, const char *string,
                                      char *result, size_t maxsize, int read_value);
unsigned long long ts_usec(const struct timespec *ts);
unsigned long long now_usec(void);

/* libudev-selinux-private.c */
#ifndef WITH_SELINUX
static inline void udev_selinux_init(struct udev *udev) {}
static inline void udev_selinux_exit(struct udev *udev) {}
static inline void udev_selinux_lsetfilecon(struct udev *udev, const char *file, unsigned int mode) {}
static inline void udev_selinux_setfscreatecon(struct udev *udev, const char *file, unsigned int mode) {}
static inline void udev_selinux_setfscreateconat(struct udev *udev, int dfd, const char *file, unsigned int mode) {}
static inline void udev_selinux_resetfscreatecon(struct udev *udev) {}
#else
void udev_selinux_init(struct udev *udev);
void udev_selinux_exit(struct udev *udev);
void udev_selinux_lsetfilecon(struct udev *udev, const char *file, unsigned int mode);
void udev_selinux_setfscreatecon(struct udev *udev, const char *file, unsigned int mode);
void udev_selinux_setfscreateconat(struct udev *udev, int dfd, const char *file, unsigned int mode);
void udev_selinux_resetfscreatecon(struct udev *udev);
#endif

#endif
