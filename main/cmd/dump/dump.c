/* SPDX-License-Identifier: ISC */
#include "dump.h"

static const struct option long_opts[] = {
	{ "dependencies", no_argument, NULL, 'd' },
	{ "list-files", required_argument, NULL, 'l' },
	{ "root", required_argument, NULL, 'r' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "dl:r:";

static int cmd_dump(int argc, char **argv)
{
	TOC_FORMAT format = TOC_FORMAT_PRETTY;
	image_entry_t *list = NULL;
	const char *root = NULL;
	int ret = EXIT_FAILURE;
	pkg_reader_t *rd;
	int i, flags = 0;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'l':
			flags |= DUMP_TOC;
			if (strcmp(optarg, "sqfs") == 0) {
				format = TOC_FORMAT_SQFS;
			} else if (strcmp(optarg, "initrd") == 0) {
				format = TOC_FORMAT_INITRD;
			} else if (strcmp(optarg, "pkg") == 0) {
				format = TOC_FORMAT_PKG;
			} else if (strcmp(optarg, "detail") == 0) {
				format = TOC_FORMAT_PRETTY;
			} else {
				fprintf(stderr, "unknown format '%s'\n",
					optarg);
				tell_read_help(argv[0]);
				return EXIT_FAILURE;
			}
			break;
		case 'r':
			root = optarg;
			break;
		case 'd':
			flags |= DUMP_DEPS;
			break;
		default:
			tell_read_help(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (flags == 0)
		flags = DUMP_ALL;

	if (optind >= argc) {
		fputs("missing argument: package file\n", stderr);
		tell_read_help(argv[0]);
		return EXIT_FAILURE;
	}

	rd = pkg_reader_open(argv[optind++]);
	if (rd == NULL)
		return EXIT_FAILURE;

	if (optind < argc)
		fputs("warning: ignoring extra arguments\n", stderr);

	if (flags & DUMP_DEPS) {
		if (dump_header(rd, flags))
			goto out;
	}

	if (image_entry_list_from_package(rd, &list))
		goto out;

	if (flags & DUMP_TOC && list != NULL) {
		if (dump_toc(list, root, format))
			goto out;
	}

	ret = EXIT_SUCCESS;
out:
	if (list != NULL)
		image_entry_free_list(list);
	pkg_reader_close(rd);
	return ret;
}

static command_t dump = {
	.cmd = "dump",
	.usage = "[OPTIONS...] <pkgfile>",
	.s_desc = "read dump all information from a package file",
	.l_desc =
"Parse a package file and dump information it contains, such as the list of\n"
"files, et cetera.\n"
"\n"
"Possible options:\n"
"  --dependencies, -d     Show dependency information of a package.\n"
"\n"
"  --list-files, -l <format>  Produce a list of files with a specified\n"
"                             formating.\n"
"\n"
"                             If \"detail\" is specified, a human readable,\n"
"                             pretty printed format with details is used.\n"
"\n"
"                             If \"sqfs\" is specified, a squashfs pseudo\n"
"                             file is genareated for setting permissions\n"
"                             bits and ownership appropriately.\n"
"\n"
"                             If \"initrd\" is specified, the format of\n"
"                             Linux gen_init_cpio is produced.\n"
"\n"
"  --root, -r <path>          If a format is used that requires absoulute\n"
"                             input paths (e.g. initrd), prefix all file\n"
"                             paths with this. Can be used, for instance to\n"
"                             unpack a package to a staging directory and\n"
"                             generate a a listing for Linux\n"
"                             CONFIG_INITRAMFS_SOURCE.\n",
	.run_cmd = cmd_dump,
};

REGISTER_COMMAND(dump)
