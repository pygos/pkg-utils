/* SPDX-License-Identifier: ISC */
#ifndef PKGLIST_H
#define PKGLIST_H

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

#endif /* PKGLIST_H */
