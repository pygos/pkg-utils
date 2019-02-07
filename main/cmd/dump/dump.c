#include "dump.h"

static const struct option long_opts[] = {
	{ "dependencies", no_argument, NULL, 'd' },
	{ "list-files", no_argument, NULL, 'l' },
	{ "format", required_argument, NULL, 'f' },
	{ "root", required_argument, NULL, 'r' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "dlf:r:";

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
		case 'f':
			if (strcmp(optarg, "sqfs") == 0) {
				format = TOC_FORMAT_SQFS;
			} else if (strcmp(optarg, "initrd") == 0) {
				format = TOC_FORMAT_INITRD;
			} else if (strcmp(optarg, "pkg") == 0) {
				format = TOC_FORMAT_PKG;
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
		case 'l':
			flags |= DUMP_TOC;
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
"  --list-files, -l       Produce a list of files. The format for the list\n"
"                         can be specified with --format.\n"
"\n"
"  --format, -f <format>  Specify what format to use for printing the table\n"
"                         of contents. Default is a pretty printed, human\n"
"                         readable version.\n"
"\n"
"                         If \"sqfs\" is specified, a squashfs pseudo file\n"
"                         is genareated for setting permissions bits and\n"
"                         ownership appropriately.\n"
"\n"
"                         If \"initrd\" is specified, a format appropriate\n"
"                         for Linux gen_init_cpio is produced.\n"
"\n"
"  --root, -r <path>      If a format is used that requires absoulute input\n"
"                         paths (e.g. initrd), prefix all file paths with\n"
"                         this. Can be used, for instance to unpack a\n"
"                         package to a staging directory and generate a\n"
"                         a listing for Linux CONFIG_INITRAMFS_SOURCE.\n",
	.run_cmd = cmd_dump,
};

REGISTER_COMMAND(dump)
