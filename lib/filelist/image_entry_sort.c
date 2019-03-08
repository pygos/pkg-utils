/* SPDX-License-Identifier: ISC */
#include <sys/stat.h>
#include <string.h>

#include "filelist/image_entry.h"

static int compare_ent(image_entry_t *a, image_entry_t *b)
{
	/* directories < .. < symlinks < devices < .. < files */
	switch (a->mode & S_IFMT) {
	case S_IFDIR:
		if (S_ISDIR(b->mode))
			goto out_len;
		return -1;
	case S_IFREG:
		if (S_ISREG(b->mode))
			goto out_fsize;
		return 1;
	case S_IFLNK:
		if (S_ISLNK(b->mode))
			goto out_len;
		if (S_ISDIR(b->mode))
			return 1;
		return -1;
	case S_IFBLK:
	case S_IFCHR:
		if (S_ISDIR(b->mode) || S_ISLNK(b->mode))
			return 1;
		if (S_ISBLK(b->mode) || S_ISCHR(b->mode))
			goto out_len;
		return -1;
	}
out_len:
	return (int)strlen(a->name) - (int)strlen(b->name);
out_fsize:
	if (a->data.file.size > b->data.file.size)
		return 1;
	if (a->data.file.size < b->data.file.size)
		return -1;
	return 0;
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
