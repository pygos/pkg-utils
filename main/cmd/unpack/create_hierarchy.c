#include "unpack.h"

int create_hierarchy(image_entry_t *list)
{
	image_entry_t *ent;

	for (ent = list; ent != NULL; ent = ent->next) {
		if (S_ISDIR(ent->mode)) {
			if (mkdir(ent->name, 0755)) {
				perror(ent->name);
				return -1;
			}
		}
	}

	for (ent = list; ent != NULL; ent = ent->next) {
		if (S_ISLNK(ent->mode)) {
			if (symlink(ent->data.symlink.target, ent->name)) {
				perror(ent->name);
				return -1;
			}
		}
	}

	return 0;
}
