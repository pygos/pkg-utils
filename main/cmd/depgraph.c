/* SPDX-License-Identifier: ISC */
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "depgraph.h"
#include "command.h"

static const struct option long_opts[] = {
	{ "no-dependencies", no_argument, NULL, 'd' },
	{ "repo-dir", required_argument, NULL, 'R' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "dR:";

static void print_dot_graph(struct pkg_dep_list *list)
{
	struct pkg_dep_node *it;
	size_t i;

	printf("digraph dependencies {\n");

	for (it = list->head; it != NULL; it = it->next) {
		for (i = 0; i < it->num_deps; ++i) {
			printf("\t\"%s\" -> \"%s\";\n",
			       it->name, it->deps[i]->name);
		}
	}

	printf("}\n");
}

static int cmd_depgraph(int argc, char **argv)
{
	int i, repofd = AT_FDCWD, ret = EXIT_FAILURE;
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
	}

	print_dot_graph(&list);
	ret = EXIT_SUCCESS;
out:
	if (repofd != AT_FDCWD)
		close(repofd);
	pkg_list_cleanup(&list);
	return ret;
}

static command_t depgraph = {
	.cmd = "depgraph",
	.usage = "[OPTIONS...] PACKAGES...",
	.s_desc = "produces a DOT language dependency graph",
	.l_desc =
"Fetches a number of packages from a repository directory by name and\n"
"produces a DOT language dependency graph for the packages.\n"
"\n"
"Possible options:\n"
"  --repo-dir, -R <path>     Specify the input repository path to fetch the\n"
"                            packages from.\n"
"  --no-dependencies, -d     Do not resolve dependencies, only process the\n"
"                            packages listed on the command line.\n",
	.run_cmd = cmd_depgraph,
};

REGISTER_COMMAND(depgraph)
