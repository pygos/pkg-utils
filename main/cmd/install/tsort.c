#include "install.h"

int sort_by_dependencies(struct pkg_dep_list *list)
{
	struct pkg_dep_node *it, *prev, *pkg;
	struct pkg_dep_list result;
	size_t i;

	result.head = NULL;
	result.tail = NULL;

	while (list->head != NULL) {
		/* find node with no outgoing edges */
		prev = NULL;
		it = list->head;

		while (it != NULL && it->num_deps != 0) {
			prev = it;
			it = it->next;
		}

		if (it == NULL) {
			fputs("cycle detected in dependency graph\n", stderr);
			pkg_list_cleanup(&result);
			return -1;
		}

		/* remove from graph */
		if (prev == NULL) {
			list->head = it->next;
		} else {
			prev->next = it->next;
		}

		if (it == list->tail)
			list->tail = prev;

		/* remove edges pointing to the package */
		pkg = it;

		for (it = list->head; it != NULL; it = it->next) {
			for (i = 0; i < it->num_deps; ++i) {
				if (it->deps[i] == pkg) {
					it->deps[i] =
						it->deps[it->num_deps - 1];
					it->num_deps -= 1;
					--i;
				}
			}

			if (it->num_deps == 0 && it->deps != NULL) {
				free(it->deps);
				it->deps = NULL;
			}
		}

		/* append to list */
		if (result.tail == NULL) {
			result.head = result.tail = pkg;
		} else {
			result.tail->next = pkg;
			result.tail = pkg;
		}
	}

	list->head = result.head;
	list->tail = result.tail;
	return 0;
}
