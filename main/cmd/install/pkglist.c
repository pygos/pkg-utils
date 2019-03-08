/* SPDX-License-Identifier: ISC */
#include "install.h"

struct pkg_dep_node *append_pkg(struct pkg_dep_list *list, const char *name)
{
	struct pkg_dep_node *new = calloc(1, sizeof(*new));

	if (new == NULL)
		goto fail_oom;

	new->name = strdup(name);
	if (new->name == NULL)
		goto fail_oom;

	if (list->tail == NULL) {
		list->head = list->tail = new;
	} else {
		list->tail->next = new;
		list->tail = new;
	}
	return new;
fail_oom:
	fputs("out of memory\n", stderr);
	free(new);
	return NULL;
}

struct pkg_dep_node *find_pkg(struct pkg_dep_list *list, const char *name)
{
	struct pkg_dep_node *it = list->head;

	while (it != NULL && strcmp(it->name, name) != 0)
		it = it->next;

	return it;
}

void pkg_list_cleanup(struct pkg_dep_list *list)
{
	struct pkg_dep_node *node;

	while (list->head != NULL) {
		node = list->head;
		list->head = node->next;

		free(node->deps);
		free(node->name);
		free(node);
	}

	list->tail = NULL;
}
