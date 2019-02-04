#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include "util/util.h"

#include "pkgreader.h"
#include "command.h"
#include "pkgio.h"

static const struct option long_opts[] = {
	{ "root", required_argument, NULL, 'r' },
	{ "no-chown", no_argument, NULL, 'o' },
	{ "no-chmod", no_argument, NULL, 'm' },
	{ "no-symlinks", no_argument, NULL, 'L' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "r:omL";

static int cmd_unpack(int argc, char **argv)
{
	const char *root = NULL, *filename;
	int i, rootfd, flags = 0;
	pkg_reader_t *rd;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'r':
			root = optarg;
			break;
		case 'L':
			flags |= UNPACK_NO_SYMLINKS;
			break;
		case 'o':
			flags |= UNPACK_NO_CHOWN;
			break;
		case 'm':
			flags |= UNPACK_NO_CHMOD;
			break;
		default:
			tell_read_help(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (optind >= argc) {
		fputs("missing argument: package file\n", stderr);
		return EXIT_FAILURE;
	}

	filename = argv[optind++];

	if (optind < argc)
		fputs("warning: ignoring extra arguments\n", stderr);

	if (root == NULL) {
		rootfd = AT_FDCWD;
	} else {
		if (mkdir_p(root))
			return EXIT_FAILURE;

		rootfd = open(root, O_RDONLY | O_DIRECTORY);
		if (rootfd < 0) {
			perror(root);
			return EXIT_FAILURE;
		}
	}

	rd = pkg_reader_open(filename);
	if (rd == NULL)
		goto fail_rootfd;

	if (pkg_unpack(rootfd, flags, rd))
		goto fail;

	pkg_reader_close(rd);
	if (rootfd != AT_FDCWD)
		close(rootfd);
	return EXIT_SUCCESS;
fail:
	pkg_reader_close(rd);
fail_rootfd:
	if (rootfd != AT_FDCWD)
		close(rootfd);
	return EXIT_FAILURE;
}

static command_t unpack = {
	.cmd = "unpack",
	.usage = "[OPTIONS...] <pkgfile>",
	.s_desc = "unpack the contents of a package file",
	.l_desc =
"The unpack command extracts the file hierarchy stored in a package into\n"
"a destination directory (default: current working directory).\n"
"\n"
"Possible options:\n"
"  --root, -r <directory>  A root directory to unpack the package. Defaults\n"
"                          to the current working directory if not set.\n"
"  --no-chown, -o          Do not change ownership of the extracted data.\n"
"                          Keep the uid/gid of the user who runs the program.\n"
"  --no-chmod, -m          Do not change permission flags of the extarcted\n"
"                          data. Use 0644 for all files and 0755 for all\n"
"                          directories.\n",
	.run_cmd = cmd_unpack,
};

REGISTER_COMMAND(unpack)
