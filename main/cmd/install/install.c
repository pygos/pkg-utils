#include "install.h"

static const struct option long_opts[] = {
	{ "root", required_argument, NULL, 'r' },
	{ "no-chown", required_argument, NULL, 'o' },
	{ "no-chmod", required_argument, NULL, 'm' },
	{ "repo-dir", required_argument, NULL, 'R' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "R:r:om";

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

static int cmd_install(int argc, char **argv)
{
	int i, rootfd = AT_FDCWD, repofd = AT_FDCWD, flags = 0;
	struct pkg_dep_list list;
	int ret = EXIT_FAILURE;

	memset(&list, 0, sizeof(list));

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
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

	if (collect_dependencies(repofd, &list))
		goto out;

	if (sort_by_dependencies(&list))
		goto out;

	if (unpack_packages(repofd, rootfd, flags, &list))
		goto out;

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
"                         directories.\n",
	.run_cmd = cmd_install,
};

REGISTER_COMMAND(install)
