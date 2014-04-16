#include <libudev.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <time.h>
#include <stdio.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


static int device_queued   (struct udev *udev, const char *path);
static int matching_device (struct udev_device *device, const char *path);

static void alarm_handler (int signum);


int
main (int   argc,
      char *argv[])
{
	const char *         devpath;
	char                 path[PATH_MAX];
	int                  timeout;
	struct udev *        udev;
	struct udev_monitor *udev_monitor;
	struct stat          devstat;
	struct udev_device * udev_device;
	const char *         type;
	int		     flags;

	if (argc != 3) {
		fprintf (stderr, "Usage: %s DEVICE TIMEOUT\n", argv[0]);
		exit (2);
	}

	devpath = argv[1];
	if (! strncmp (devpath, "UUID=", 5)) {
		strcpy (path, "/dev/disk/by-uuid/");
		strcat (path, devpath + 5);
	} else if (! strncmp (devpath, "LABEL=", 6)) {
		strcpy (path, "/dev/disk/by-label/");
		strcat (path, devpath + 6);
	} else {
		strcpy (path, devpath);
	}

	timeout = atoi (argv[2]);

	signal (SIGALRM, alarm_handler);
	alarm (timeout);

	/* Connect to the udev monitor first; if we stat() first, the
	 * event might happen between the stat() and the time we actually
	 * get hooked up.
	 */
	udev = udev_new ();
	udev_monitor = udev_monitor_new_from_netlink (udev, "udev");

	udev_monitor_filter_add_match_subsystem_devtype (udev_monitor, "block", NULL);
	udev_monitor_enable_receiving (udev_monitor);

	/* If the device is not being processed, check to see whether it
	 * exists already on the filesystem.  If this is true, we don't need
	 * to wait for it can obtain the filesystem type by looking up the
	 * udevdb record by major/minor.
	 */
	if ((! device_queued (udev, devpath))
	    && (stat (path, &devstat) == 0)
	    && S_ISBLK (devstat.st_mode))
	{
		udev_device = udev_device_new_from_devnum (udev, 'b', devstat.st_rdev);
		if (udev_device) {
			type = udev_device_get_property_value (udev_device, "ID_FS_TYPE");
			if (type) {
				printf ("%s\n", type);

				udev_device_unref (udev_device);
				goto exit;
			}

			udev_device_unref (udev_device);
		}
	}

	/* udev monitor socket is nonblocking by default, but we want to wait
	 * for events */
	flags = fcntl (udev_monitor_get_fd (udev_monitor), F_GETFL, 0);
	flags &= ~O_NONBLOCK;
	fcntl (udev_monitor_get_fd (udev_monitor), F_SETFL, flags);

	/* When the device doesn't exist yet, or is still being processed
	 * by udev, use the monitor socket to wait it to be done.
	 */
	while ((udev_device = udev_monitor_receive_device (udev_monitor)) != NULL) {
		if (matching_device (udev_device, devpath)) {
			type = udev_device_get_property_value (udev_device, "ID_FS_TYPE");
			if (type) {
				printf ("%s\n", type);

				udev_device_unref (udev_device);
				goto exit;
			}

		}

		udev_device_unref (udev_device);
	}

exit:
	udev_monitor_unref (udev_monitor);
	udev_unref (udev);

	exit (0);
}


static int
device_queued (struct udev *udev,
	       const char * devpath)
{
	struct udev_queue *     udev_queue;
	struct udev_list_entry *queue_entry;
	int                     found = 0;

	udev_queue = udev_queue_new (udev);

	for (queue_entry = udev_queue_get_queued_list_entry (udev_queue);
	     queue_entry != NULL;
	     queue_entry = udev_list_entry_get_next (queue_entry)) {
		const char *        syspath;
		struct udev_device *udev_device;

		syspath = udev_list_entry_get_name (queue_entry);
		udev_device = udev_device_new_from_syspath (udev, syspath);
		if (udev_device) {
			if (matching_device (udev_device, devpath))
				found = 1;

			udev_device_unref (udev_device);
		}
	}

	udev_queue_unref (udev_queue);

	return found;
}

static int
matching_device (struct udev_device *device,
		 const char *        path)
{
	const char *            devnode;
	struct udev_list_entry *devlinks_entry;

	/* Match by name */
	devnode = udev_device_get_devnode (device);
	if (devnode && (! strcmp (path, devnode)))
		return 1;

	/* Match by UUID */
	if (! strncmp (path, "UUID=", 5)) {
		const char *uuid;

		uuid = udev_device_get_property_value (device, "ID_FS_UUID");
		if (uuid && (! strcmp (path + 5, uuid)))
			return 1;
	}

	/* Match by LABEL */
	if (! strncmp (path, "LABEL=", 6)) {
		const char *label;

		label = udev_device_get_property_value (device, "ID_FS_LABEL");
		if (label && (! strcmp (path + 6, label)))
			return 1;
	}

	/* Match by symlink */
	for (devlinks_entry = udev_device_get_devlinks_list_entry (device);
	     devlinks_entry != NULL;
	     devlinks_entry = udev_list_entry_get_next (devlinks_entry))
		if (! strcmp (path, udev_list_entry_get_name (devlinks_entry)))
			return 1;

	return 0;
}


static void
alarm_handler (int signum)
{
	exit (1);
}
