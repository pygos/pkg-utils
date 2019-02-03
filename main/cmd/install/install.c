#include "install.h"

static const struct option long_opts[] = {
	{ "root", required_argument, NULL, 'r' },
	{ "no-chown", no_argument, NULL, 'o' },
	{ "no-chmod", no_argument, NULL, 'm' },
	{ "no-dependencies", no_argument, NULL, 'd' },
	{ "repo-dir", required_argument, NULL, 'R' },
	{ "list-packages", no_argument, NULL, 'p' },
	{ "list-files", no_argument, NULL, 'l' },
	{ "format", required_argument, NULL, 'F' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "r:omdR:plF:";

static int unpack_packages(int repofd, int rootfd, int flags,
			   struct pkg_dep_list *list)
{
	struct pkg_dep_node *it;
	pkg_reader_t *rd;

	for (it = list->head; it != NULL; it = it->next) {
		rd = pkg_reader_open_repo(repofd, it->name);
		if (rd == NULL)
			return -1;

		if (pkg_unpack(rootfd, flags, rd)) {
			pkg_reader_close(rd);
			return -1;
		}

		pkg_reader_close(rd);
	}

	return 0;
}

static void list_packages(struct pkg_dep_list *list)
{
	struct pkg_dep_node *it;

	for (it = list->head; it != NULL; it = it->next)
		printf("%s\n", it->name);
}

static int list_files(int repofd, const char *rootdir, TOC_FORMAT format,
		      struct pkg_dep_list *list)
{
	struct pkg_dep_node *it;
	image_entry_t *toc;
	pkg_reader_t *rd;

	for (it = list->head; it != NULL; it = it->next) {
		rd = pkg_reader_open_repo(repofd, it->name);
		if (rd == NULL)
			return -1;

		toc = image_entry_list_from_package(rd);
		if (toc == NULL) {
			pkg_reader_close(rd);
			return -1;
		}

		if (dump_toc(toc, rootdir, format)) {
			image_entry_free_list(toc);
			pkg_reader_close(rd);
			return -1;
		}

		image_entry_free_list(toc);
		pkg_reader_close(rd);
	}

	return 0;
}

static int cmd_install(int argc, char **argv)
{
	int i, rootfd = AT_FDCWD, repofd = AT_FDCWD, flags = 0;
	int ret = EXIT_FAILURE, mode = INSTALL_MODE_INSTALL;
	TOC_FORMAT format = TOC_FORMAT_PRETTY;
	const char *rootdir = NULL;
	struct pkg_dep_list list;
	bool resolve_deps = true;

	memset(&list, 0, sizeof(list));

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'd':
			resolve_deps = false;
			break;
		case 'p':
			mode = INSTALL_MODE_LIST_PKG;
			break;
		case 'l':
			mode = INSTALL_MODE_LIST_FILES;
			break;
		case 'F':
			if (strcmp(optarg, "sqfs") == 0) {
				format = TOC_FORMAT_SQFS;
			} else if (strcmp(optarg, "initrd") == 0) {
				format = TOC_FORMAT_INITRD;
			} else {
				fprintf(stderr, "unknown format '%s'\n",
					optarg);
				goto out;
			}
			break;
		case 'R':
			if (repofd != AT_FDCWD) {
				fputs("repo specified more than once\n",
				      stderr);
				tell_read_help(argv[0]);
				goto out;
			}
			repofd = open(optarg, O_RDONLY | O_DIRECTORY);
			if (repofd < 0) {
				perror(optarg);
				goto out;
			}
			break;
		case 'r':
			if (rootfd != AT_FDCWD) {
				fputs("root specified more than once\n",
				      stderr);
				tell_read_help(argv[0]);
				goto out;
			}

			if (mkdir_p(optarg))
				goto out;

			rootfd = open(optarg, O_RDONLY | O_DIRECTORY);
			if (rootfd < 0) {
				perror(optarg);
				goto out;
			}

			rootdir = optarg;
			break;
		case 'o':
			flags |= UNPACK_NO_CHOWN;
			break;
		case 'm':
			flags |= UNPACK_NO_CHMOD;
			break;
		default:
			tell_read_help(argv[0]);
			goto out;
		}
	}

	for (i = optind; i < argc; ++i) {
		if (append_pkg(&list, argv[i]) == NULL)
			goto out;
	}

	if (resolve_deps) {
		if (collect_dependencies(repofd, &list))
			goto out;

		if (sort_by_dependencies(&list))
			goto out;
	}

	switch (mode) {
	case INSTALL_MODE_LIST_PKG:
		list_packages(&list);
		break;
	case INSTALL_MODE_LIST_FILES:
		if (list_files(repofd, rootdir, format, &list))
			goto out;
		break;
	default:
		if (unpack_packages(repofd, rootfd, flags, &list))
			goto out;
		break;
	}

	ret = EXIT_SUCCESS;
out:
	if (rootfd != AT_FDCWD)
		close(rootfd);
	if (repofd != AT_FDCWD)
		close(repofd);
	pkg_list_cleanup(&list);
	return ret;
}

static command_t install = {
	.cmd = "install",
	.usage = "[OPTIONS...] PACKAGES...",
	.s_desc = "install packages and their dependencies",
	.l_desc =
"The install command fetches a number of packages from a repository directory\n"
"by name and extracts them to a specified root directory. Dependencies of the\n"
"packages are evaluated and also installed.\n"
"\n"
"Possible options:\n"
"  --repo-dir, -R <path>  Specify the input repository path to fetch the\n"
"                         packages from.\n"
"  --root, -r <path>      A root directory to unpack the package. Defaults\n"
"                         to the current working directory if not set.\n"
"  --no-chown, -o         Do not change ownership of the extracted data.\n"
"                         Keep the uid/gid of the user who runs the program.\n"
"  --no-chmod, -m         Do not change permission flags of the extarcted\n"
"                         data. Use 0644 for all files and 0755 for all\n"
"                         directories.\n"
"  --no-dependencies, -d  Do not resolve dependencies, only install files\n"
"                         packages listed on the command line.\n"
"  --list-packages, -p    Do not install packages, print out final package\n"
"                         list that would be installed.\n"
"  --list-files, -l       Do not install packages, print out table of\n"
"                         contents for all packages that would be installed.\n"
"  --format, -F <format>  Specify what format to use for printing the table\n"
"                         of contents. Default is a pretty printed, human\n"
"                         readable version.\n"
"\n"
"                         If \"sqfs\" is specified, a squashfs pseudo file\n"
"                         is genareated for setting permissions bits and\n"
"                         ownership appropriately.\n"
"\n"
"                         If \"initrd\" is specified, a format appropriate\n"
"                         for Linux gen_init_cpio is produced.\n",
	.run_cmd = cmd_install,
};

REGISTER_COMMAND(install)
