#ifndef IMAGE_ENTRY_H
#define IMAGE_ENTRY_H

#include <sys/types.h>
#include <stdint.h>

#include "pkgreader.h"

typedef struct image_entry_t {
	struct image_entry_t *next;
	char *name;
	mode_t mode;
	uid_t uid;
	gid_t gid;

	union {
		struct {
			char *location;
			uint64_t size;
			uint32_t id;
		} file;

		struct {
			char *target;
		} symlink;
	} data;
} image_entry_t;

void image_entry_free(image_entry_t *ent);

void image_entry_free_list(image_entry_t *list);

image_entry_t *image_entry_sort(image_entry_t *list);

#endif /* IMAGE_ENTRY_H */
