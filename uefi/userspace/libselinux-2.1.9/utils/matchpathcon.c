#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <selinux/selinux.h>
#include <limits.h>
#include <stdlib.h>


void usage(const char *progname)
{
	fprintf(stderr,
		"usage:  %s [-N] [-n] [-f file_contexts] [-p prefix] [-Vq] path...\n",
		progname);
	exit(1);
}

int printmatchpathcon(char *path, int header, int mode)
{
	char *buf;
	int rc = matchpathcon(path, mode, &buf);
	if (rc < 0) {
		if (errno == ENOENT) {
			buf=strdup("<<none>>");
		} else {
			fprintf(stderr, "matchpathcon(%s) failed: %s\n", path,
				strerror(errno));
			return 1;
		}
	}
	if (header)
		printf("%s\t%s\n", path, buf);
	else
		printf("%s\n", buf);

	freecon(buf);
	return 0;
}

int main(int argc, char **argv)
{
	int i, init = 0;
	int header = 1, opt;
	int verify = 0;
	int notrans = 0;
	int error = 0;
	int quiet = 0;

	if (argc < 2)
		usage(argv[0]);

	while ((opt = getopt(argc, argv, "Nnf:p:Vq")) > 0) {
		switch (opt) {
		case 'n':
			header = 0;
			break;
		case 'V':
			verify = 1;
			break;
		case 'N':
			notrans = 1;
			set_matchpathcon_flags(MATCHPATHCON_NOTRANS);
			break;
		case 'f':
			if (init) {
				fprintf(stderr,
					"%s:  -f and -p are exclusive\n",
					argv[0]);
				exit(1);
			}
			init = 1;
			if (matchpathcon_init(optarg)) {
				fprintf(stderr,
					"Error while processing %s:  %s\n",
					optarg,
					errno ? strerror(errno) : "invalid");
				exit(1);
			}
			break;
		case 'p':
			if (init) {
				fprintf(stderr,
					"%s:  -f and -p are exclusive\n",
					argv[0]);
				exit(1);
			}
			init = 1;
			if (matchpathcon_init_prefix(NULL, optarg)) {
				fprintf(stderr,
					"Error while processing %s:  %s\n",
					optarg,
					errno ? strerror(errno) : "invalid");
				exit(1);
			}
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			usage(argv[0]);
		}
	}
	for (i = optind; i < argc; i++) {
		int rc, mode = 0;
		struct stat buf;
		char *path = argv[i];
		int len = strlen(path);
		if (len > 1  && path[len - 1 ] == '/')
			path[len - 1 ] = '\0';

		if (lstat(path, &buf) == 0)
			mode = buf.st_mode;

		if (verify) {
			rc = selinux_file_context_verify(path, mode);

			if (quiet) {
				if (rc == 1)
					continue;
				else
					exit(1);
			}

			if (rc == -1) {
				printf("%s error: %s\n", path, strerror(errno));
				exit(1);
			} else if (rc == 1) {
				printf("%s verified.\n", path);
			} else {
				security_context_t con;
				error = 1;
				if (notrans)
					rc = lgetfilecon_raw(path, &con);
				else
					rc = lgetfilecon(path, &con);

				if (rc >= 0) {
					printf("%s has context %s, should be ",
					       path, con);
					printmatchpathcon(path, 0, mode);
					freecon(con);
				} else {
					printf
					    ("actual context unknown: %s, should be ",
					     strerror(errno));
					printmatchpathcon(path, 0, mode);
				}
			}
		} else {
			error |= printmatchpathcon(path, header, mode);
		}
	}
	matchpathcon_fini();
	return error;
}
