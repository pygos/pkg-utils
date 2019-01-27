#include "pack.h"

static const struct {
	const char *name;
	image_entry_t *(*handle)(input_file_t *f);
} line_hooks[] = {
	{ "file", filelist_mkfile },
	{ "dir", filelist_mkdir },
	{ "slink", filelist_mkslink },
};

#define NUM_LINE_HOOKS (sizeof(line_hooks) / sizeof(line_hooks[0]))

image_entry_t *filelist_read(const char *filename)
{
	image_entry_t *ent, *list = NULL;
	input_file_t f;
	size_t i, len;
	char *ptr;
	int ret;

	if (open_file(&f, filename))
		return NULL;

	for (;;) {
		ret = prefetch_line(&f);
		if (ret < 0)
			goto fail;
		if (ret > 0)
			break;

		ptr = f.line;

		for (i = 0; i < NUM_LINE_HOOKS; ++i) {
			len = strlen(line_hooks[i].name);

			if (strncmp(ptr, line_hooks[i].name, len) != 0)
				continue;
			if (!isspace(ptr[len]) && ptr[len] != '\0')
				continue;
			for (ptr += len; isspace(*ptr); ++ptr)
				;
			memmove(f.line, ptr, strlen(ptr) + 1);
			break;
		}

		if (i == NUM_LINE_HOOKS) {
			fprintf(stderr, "%s: %zu: unknown entry type\n",
				f.filename, f.linenum);
			goto fail;
		}

		ent = line_hooks[i].handle(&f);
		if (ent == NULL)
			goto fail;

		ent->next = list;
		list = ent;
	}

	if (list == NULL) {
		fprintf(stderr, "%s: does not contain any entries\n",
			f.filename);
		goto fail;
	}

	cleanup_file(&f);
	return list;
fail:
	cleanup_file(&f);
	image_entry_free_list(list);
	return NULL;
}
