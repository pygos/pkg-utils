#include "pack.h"

static int handle_requires(input_file_t *f, void *obj)
{
	char *ptr = f->line, *end;
	pkg_desc_t *desc = obj;
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
			input_file_complain(f, "dependency name too long");
			return -1;
		}

		dep = calloc(1, sizeof(*dep) + len + 1);
		if (dep == NULL) {
			input_file_complain(f, "out of memory");
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

static int handle_name(input_file_t *f, void *obj)
{
	pkg_desc_t *desc = obj;

	if (desc->name != NULL) {
		input_file_complain(f, "name redefined");
		return -1;
	}

	desc->name = strdup(f->line);
	if (desc->name == NULL) {
		input_file_complain(f, "out of memory");
		return -1;
	}

	return 0;
}

static const keyword_handler_t line_hooks[] = {
	{ "requires", handle_requires },
	{ "name", handle_name },
};

#define NUM_LINE_HOOKS (sizeof(line_hooks) / sizeof(line_hooks[0]))

int desc_read(const char *path, pkg_desc_t *desc)
{
	input_file_t f;

	memset(desc, 0, sizeof(*desc));

	if (open_file(&f, path))
		return -1;

	if (process_file(&f, line_hooks, NUM_LINE_HOOKS, desc)) {
		cleanup_file(&f);
		return -1;
	}

	cleanup_file(&f);
	return 0;
}

void desc_free(pkg_desc_t *desc)
{
	dependency_t *dep;

	while (desc->deps != NULL) {
		dep = desc->deps;
		desc->deps = dep->next;

		free(dep);
	}

	free(desc->name);
}
