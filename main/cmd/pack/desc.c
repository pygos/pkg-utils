#include "pack.h"

static int handle_requires(input_file_t *f, pkg_desc_t *desc)
{
	char *ptr = f->line, *end;
	dependency_t *dep;
	size_t len;

	while (*ptr != '\0') {
		while (isspace(*ptr))
			++ptr;
		end = ptr;
		while (*end != '\0' && !isspace(*end))
			++end;

		len = end - ptr;
		if (len == 0)
			break;

		if (len > 0xFF) {
			fprintf(stderr, "%s: %zu: dependency name too long\n",
				f->filename, f->linenum);
			return -1;
		}

		dep = calloc(1, sizeof(*dep) + len + 1);
		if (dep == NULL) {
			fprintf(stderr, "%s: %zu: out of memory\n",
				f->filename, f->linenum);
			return -1;
		}

		memcpy(dep->name, ptr, len);
		dep->type = PKG_DEPENDENCY_REQUIRES;
		dep->next = desc->deps;
		desc->deps = dep;

		ptr = end;
	}

	return 0;
}

static const struct {
	const char *name;
	int (*handle)(input_file_t *f, pkg_desc_t *desc);
} line_hooks[] = {
	{ "requires", handle_requires },
};

#define NUM_LINE_HOOKS (sizeof(line_hooks) / sizeof(line_hooks[0]))

int desc_read(const char *path, pkg_desc_t *desc)
{
	input_file_t f;
	size_t i, len;
	char *ptr;
	int ret;

	memset(desc, 0, sizeof(*desc));

	if (open_file(&f, path))
		return -1;

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
			fprintf(stderr, "%s: %zu: unknown description field\n",
				f.filename, f.linenum);
			goto fail;
		}

		ret = line_hooks[i].handle(&f, desc);
		if (ret)
			goto fail;
	}

	return 0;
fail:
	return -1;
}

void desc_free(pkg_desc_t *desc)
{
	dependency_t *dep;

	while (desc->deps != NULL) {
		dep = desc->deps;
		desc->deps = dep->next;

		free(dep);
	}
}
