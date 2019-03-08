/* SPDX-License-Identifier: ISC */
#include <stdlib.h>
#include <stdio.h>

#include "depgraph.h"

static void remove_dependency(struct pkg_dep_list *list,
			      struct pkg_dep_node *pkg)
{
	struct pkg_dep_node *it;
	size_t i;

	for (it = list->head; it != NULL; it = it->next) {
		for (i = 0; i < it->num_deps; ++i) {
			if (it->deps[i] == pkg) {
				it->deps[i] = it->deps[it->num_deps - 1];
				it->num_deps -= 1;
				--i;
			}
		}

		if (it->num_deps == 0 && it->deps != NULL) {
			free(it->deps);
			it->deps = NULL;
		}
	}
}

static void append(struct pkg_dep_list *out, struct pkg_dep_node *pkg)
{
	if (out->tail == NULL) {
		out->head = out->tail = pkg;
	} else {
		out->tail->next = pkg;
		out->tail = pkg;
	}
}

static struct pkg_dep_node *remove_no_deps(struct pkg_dep_list *list)
{
	struct pkg_dep_node *it = list->head, *prev = NULL;

	while (it != NULL && it->num_deps != 0) {
		prev = it;
		it = it->next;
	}

	if (it != NULL) {
		if (prev == NULL) {
			list->head = it->next;
		} else {
			prev->next = it->next;
		}

		if (it == list->tail)
			list->tail = prev;
	}

	return it;
}

int sort_by_dependencies(struct pkg_dep_list *list)
{
	struct pkg_dep_list result = { NULL, NULL };
	struct pkg_dep_node *pkg;

	while (list->head != NULL) {
		pkg = remove_no_deps(list);

		if (pkg == NULL) {
			fputs("cycle detected in dependency graph\n", stderr);
			pkg_list_cleanup(&result);
			return -1;
		}

		remove_dependency(list, pkg);
		append(&result, pkg);
	}

	*list = result;
	return 0;
}
