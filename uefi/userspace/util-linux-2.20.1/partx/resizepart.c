#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

#include "c.h"
#include "nls.h"
#include "partx.h"
#include "sysfs.h"
#include "strutils.h"

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out, _(" %s <disk device> <partition number> <length>\n"),
		program_invocation_short_name);
	fputs(_("\nOptions:\n"), out);
	fputs(_(" -h, --help     display this help and exit\n"), out);
	fputs(_(" -V, --version  output version information and exit\n"), out);
	fprintf(out, _("\nFor more details see resizepart(8).\n"));
	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static struct dirent *xreaddir(DIR *dp)
{
	struct dirent *d;

	while ((d = readdir(dp))) {
		if (!strcmp(d->d_name, ".") ||
		    !strcmp(d->d_name, ".."))
			continue;

		/* blacklist here? */
		break;
	}
	return d;
}

/*
 * Converts @partno (partition number) to devno of the partition.
 * The @cxt handles wholedisk device.
 *
 * Note that this code does not expect any special format of the
 * partitions devnames.
 */
dev_t sysfs_partno_to_devno(struct sysfs_cxt *cxt, int partno)
{
	DIR *dir;
	struct dirent *d;
	char path[256];
	dev_t devno = 0;

	dir = sysfs_opendir(cxt, NULL);
	if (!dir)
		return 0;

	while ((d = xreaddir(dir))) {
		int n, maj, min;

		if (!sysfs_is_partition_dirent(dir, d, NULL))
			continue;

		snprintf(path, sizeof(path), "%s/partition", d->d_name);
		if (sysfs_read_int(cxt, path, &n))
			continue;

		if (n == partno) {
			snprintf(path, sizeof(path), "%s/dev", d->d_name);
			if (sysfs_scanf(cxt, path, "%d:%d", &maj, &min) == 2)
				devno = makedev(maj, min);
			break;
		}
	}

	closedir(dir);
	return devno;
}

static int get_partition_start(int fd, int partno, uint64_t *start)
{
	struct stat st;
	struct sysfs_cxt disk = { 0, -1, NULL, NULL },
			 part = { 0, -1, NULL, NULL };
	dev_t devno = 0;
	int rc = -1;

	/*
	 * wholedisk
	 */
	if (fstat(fd, &st) || !S_ISBLK(st.st_mode))
		goto done;
	devno = st.st_rdev;
	if (sysfs_init(&disk, devno, NULL))
		goto done;
	/*
	 * partition
	 */
	devno = sysfs_partno_to_devno(&disk, partno);
	if (!devno)
		goto done;
	if (sysfs_init(&part, devno, &disk))
		goto done;
	if (sysfs_read_u64(&part, "start", start))
		goto done;

	rc = 0;
done:
	sysfs_deinit(&part);
	sysfs_deinit(&disk);
	return rc;
}

uint64_t strtou64_or_err(const char *str, const char *errmesg)
{
	uintmax_t num;
	char *end = NULL;

	if (str == NULL || *str == '\0')
		goto err;
	errno = 0;
	num = strtoumax(str, &end, 10);

	if (errno || str == end || (end && *end))
		goto err;

	return num;
err:
	if (errno)
		err(EXIT_FAILURE, "%s: '%s'", errmesg, str);

	errx(EXIT_FAILURE, "%s: '%s'", errmesg, str);
}

uint32_t strtou32_or_err(const char *str, const char *errmesg)
{
	uint64_t num = strtou64_or_err(str, errmesg);

	if (num > UINT32_MAX)
		errx(EXIT_FAILURE, "%s: '%s'", errmesg, str);

	return num;
}

int main(int argc, char **argv)
{
	int c, fd, partno;
	const char *wholedisk;
	uint64_t start;

	static const struct option longopts[] = {
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'V'},
		{NULL, no_argument, 0, '0'},
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c = getopt_long(argc, argv, "Vh", longopts, NULL)) != -1)
		switch (c) {
		case 'V':
			printf(_("%s from %s\n"), program_invocation_short_name, PACKAGE_STRING);
			return EXIT_SUCCESS;
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}

	if (argc != 4)
		usage(stderr);

	wholedisk = argv[1];
	partno = strtou32_or_err(argv[2], _("invalid partition number argument"));

	if ((fd = open(wholedisk, O_RDONLY)) < 0)
		err(EXIT_FAILURE, _("cannot open %s"), wholedisk);

	if (get_partition_start(fd, partno, &start))
		err(EXIT_FAILURE, _("%s: failed to get start of the partition number %s"),
				wholedisk, argv[2]);

	if (partx_resize_partition(fd, partno, start,
			strtou64_or_err(argv[3], _("invalid length argument"))))
		err(EXIT_FAILURE, _("failed to resize partition"));

	return 0;
}
