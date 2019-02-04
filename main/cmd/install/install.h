#ifndef INSTALL_H
#define INSTALL_H

#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "util/util.h"

#include "image_entry.h"
#include "pkgreader.h"
#include "command.h"
#include "pkgio.h"

enum {
	INSTALL_MODE_INSTALL = 0,
	INSTALL_MODE_LIST_PKG,
	INSTALL_MODE_LIST_FILES,
};

struct pkg_dep_node {
	char *name;

	/* num dependencies == number of outgoing edges */
	size_t num_deps;

	/* direct dependencies == array of outgoing edges */
	struct pkg_dep_node **deps;

	/* linked list pointer */
	struct pkg_dep_node *next;
};

struct pkg_dep_list {
	struct pkg_dep_node *head;
	struct pkg_dep_node *tail;
};

struct pkg_dep_node *append_pkg(struct pkg_dep_list *list, const char *name);

struct pkg_dep_node *find_pkg(struct pkg_dep_list *list, const char *name);

void pkg_list_cleanup(struct pkg_dep_list *list);

int collect_dependencies(int repofd, struct pkg_dep_list *list);

int sort_by_dependencies(struct pkg_dep_list *list);

#endif /* INSTALL_H */
