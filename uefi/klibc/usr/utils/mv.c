#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/limits.h>

#define BUFFER_SIZE 32768

/* copy_file - copy a file, used on different fs mv */
static int copy_file(const char *src, const char *dest, mode_t mode)
{
	char buf[BUFFER_SIZE];
	int sfd, dfd;
	ssize_t len;

	sfd = open(src, O_RDONLY);
	if (sfd < 0)
		return -1;

	dfd = open(dest, O_WRONLY | O_CREAT, mode);
	if (dfd < 0) {
		close(sfd);
		return -1;
	}

	while ((len = read(sfd, buf, sizeof(buf))) > 0) {
		len = write(dfd, buf, len);
		if (len < 0) {
			close(sfd);
			close(dfd);
			return -1;
		}
	}

	close(sfd);
	close(dfd);
	return 0;
}

/* copy - recursively copy directories */
static int copy(char *src, const char *dest)
{
	int len;
	struct stat sb, sf;
	char target[PATH_MAX];
	char *p;

	p = strrchr(src, '/');
	if (p) {
		p++;
		/* trailing slashes case */
		if (strlen(p) == 0) {
			len = strlen(src) - 1;
			p = src;
			while (0 < len && p[len] == '/')
				p[len--] = '\0';
			p = strrchr(p, '/');
			p++;
		}
	} else {
		p = src;
	}

	memset(&sb, 0, sizeof(struct stat));
	/* might not exist yet */
	if (stat(dest, &sb) == 0) {
		if (S_ISDIR(sb.st_mode)) {
			len = snprintf(target, PATH_MAX, "%s/%s", dest, p);
			if (len  >= PATH_MAX)
				return -1;
		} else {
			len = snprintf(target, PATH_MAX, "%s/%s", dest, src);
			if (len  >= PATH_MAX)
				return -1;
		}
	} else {
		len = snprintf(target, PATH_MAX, "%s", dest);
		if (len  >= PATH_MAX)
			return -1;
	}

	if (rename(src, target) == 0) {
		return 0;
	} else {
		if (errno != EXDEV)
			return -1;
	}


	/* cross fs copy */
	memset(&sf, 0, sizeof(struct stat));
	if (stat(src, &sf) < 0)
		return -1;
	if (!S_ISDIR(sf.st_mode)) {
		len = copy_file(src, target, sf.st_mode);
		if (len == 0)
			return 0;
		else
			return -1;
	}

	DIR *dir;
	struct dirent *d;
	char path[PATH_MAX];

	if (mkdir(target, sf.st_mode) < 0)
		return -1;

	dir = opendir(src);
	if (!dir) {
		/* EACCES means we can't read it.
		 * Might be empty and removable. */
		if (errno != EACCES)
			return -1;
	}
	while ((d = readdir(dir))) {

		/* Skip . and .. */
		if (d->d_name[0] == '.' && (d->d_name[1] == '\0'
		    || (d->d_name[1] == '.' && d->d_name[2] == '\0')))
			continue;

		/* skip to long path */
		if (strlen(src) + 1 + strlen(d->d_name) >= PATH_MAX  - 1)
			continue;

		snprintf(path, sizeof path, "%s/%s", src, d->d_name);
		if (len  >= sizeof path)
			return -1;

		memset(&sf, 0, sizeof(struct stat));
		if (stat(path, &sf) < 0) {
			closedir(dir);
			return -1;
		}

		/* recursively copy files and directories */
		if (copy(path, target) < 0)
			return -1;
	}
	closedir(dir);
	return 0;
}

/* nuke - rm a file or directory recursively */
static int nuke(const char *src)
{
	struct stat sb;
	DIR *dir;
	struct dirent *d;
	char path[PATH_MAX];
	int len;

	memset(&sb, 0, sizeof(struct stat));
	/* gone, no work */
	if (stat(src, &sb) < 0)
		return 0;

	if (!S_ISDIR(sb.st_mode)) {
		if (unlink(src) == 0)
			return 0;
		else
			return 1;
	}

	dir = opendir(src);
	if (!dir) {
		/* EACCES means we can't read it.
		 * Might be empty and removable. */
		if (errno != EACCES)
			return -1;
	}
	while ((d = readdir(dir))) {

		/* Skip . and .. */
		if (d->d_name[0] == '.' && (d->d_name[1] == '\0'
		    || (d->d_name[1] == '.' && d->d_name[2] == '\0')))
			continue;

		/* skip to long path */
		if (strlen(src) + 1 + strlen(d->d_name) >= PATH_MAX  - 1)
			continue;

		len = snprintf(path, sizeof path, "%s/%s", src, d->d_name);
		if (len  >= sizeof path)
			return -1;

		memset(&sb, 0, sizeof(struct stat));
		if (stat(path, &sb) < 0) {
			closedir(dir);
			return -1;
		}

		if (nuke(path) < 0)
			return -1;
	}
	closedir(dir);
	rmdir(src);
	return 0;
}

int main(int argc, char *argv[])
{
	int c, f;
	struct stat sb;

	f = 0;
	do {
		c = getopt(argc, argv, "f");
		if (c == EOF)
			break;

		switch (c) {

		case 'f':
			f = 1;
			break;
		case '?':
			fprintf(stderr, "%s: invalid option -%c\n",
				argv[0], optopt);
			return 1;
		}

	} while (1);

	/* not enough arguments */
	if (argc - optind < 2) {
		fprintf(stderr, "Usage: %s [-f] source dest\n", argv[0]);
		return 1;
	}

	/* check on many archs if destination is a directory to mv in */
	memset(&sb, 0, sizeof(struct stat));
	if (stat(argv[argc - 1], &sb) < 0 && argc - optind > 2) {
		if (!(S_ISDIR(sb.st_mode))) {
			fprintf(stderr,
				"multiple targets and %s is not a directory\n",
				argv[argc - 1]);
			return 1;
		}
	}

	/* remove destination */
	if (f)
		nuke(argv[argc - 1]);

	/* the mv action */
	for (c = optind; c < argc - 1; c++)
		if (copy(argv[c], argv[argc - 1]) < 0) {
			perror("Could not copy file");
			return -1;
		}

	/* Only rm after sucessfull rename */
	for (c = optind; c < argc - 1; c++)
		if (nuke(argv[c]) < 0) {
			perror("Could not rm file");
			return -1;
		}
	return 0;
}
