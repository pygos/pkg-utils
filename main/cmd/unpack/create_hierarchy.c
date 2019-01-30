#include "unpack.h"

int create_hierarchy(int dirfd, image_entry_t *list)
{
	image_entry_t *ent;

	for (ent = list; ent != NULL; ent = ent->next) {
		if (S_ISDIR(ent->mode)) {
			if (mkdirat(dirfd, ent->name, 0755)) {
				perror(ent->name);
				return -1;
			}
		}
	}

	for (ent = list; ent != NULL; ent = ent->next) {
		if (S_ISLNK(ent->mode)) {
			if (symlinkat(ent->data.symlink.target,
				      dirfd, ent->name)) {
				perror(ent->name);
				return -1;
			}
		}
	}

	return 0;
}
