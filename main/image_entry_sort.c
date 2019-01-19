#include <sys/stat.h>
#include <string.h>

#include "image_entry.h"

static int compare_ent(image_entry_t *a, image_entry_t *b)
{
	if (S_ISDIR(a->mode)) {
		if (S_ISDIR(b->mode))
			goto out_len;
		return -1;
	}
	if (S_ISDIR(b->mode))
		return 1;

	if (S_ISREG(a->mode)) {
		if (S_ISREG(b->mode)) {
			if (a->data.file.size > b->data.file.size)
				return 1;
			if (a->data.file.size < b->data.file.size)
				return -1;
			return 0;
		}
		return 1;
	}
	if (S_ISREG(b->mode))
		return 1;
out_len:
	return (int)strlen(a->name) - (int)strlen(b->name);
}

static image_entry_t *insert_sorted(image_entry_t *list, image_entry_t *ent)
{
	image_entry_t *it, *prev;

	if (list == NULL || compare_ent(list, ent) > 0) {
		ent->next = list;
		return ent;
	}

	it = list->next;
	prev = list;

	while (it != NULL) {
		if (compare_ent(it, ent) > 0)
			break;

		prev = it;
		it = it->next;
	}

	prev->next = ent;
	ent->next = it;
	return list;
}

image_entry_t *image_entry_sort(image_entry_t *list)
{
	image_entry_t *sorted = NULL, *ent;

	while (list != NULL) {
		ent = list;
		list = list->next;

		sorted = insert_sorted(sorted, ent);
	}

	return sorted;
}
