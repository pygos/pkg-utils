#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>

#include "image_entry.h"

void image_entry_free(image_entry_t *ent)
{
	if (ent != NULL) {
		switch (ent->mode & S_IFMT) {
		case S_IFREG:
			free(ent->data.file.location);
			break;
		case S_IFLNK:
			free(ent->data.symlink.target);
			break;
		default:
			break;
		}

		free(ent->name);
		free(ent);
	}
}

void image_entry_free_list(image_entry_t *list)
{
	image_entry_t *ent;

	while (list != NULL) {
		ent = list;
		list = list->next;

		image_entry_free(ent);
	}
}
