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

static int handle_toc_compressor(input_file_t *f, void *obj)
{
	pkg_desc_t *desc = obj;

	desc->toccmp = compressor_by_name(f->line);

	if (desc->toccmp == NULL) {
		input_file_complain(f, "unkown compressor");
		return -1;
	}

	return 0;
}

static int handle_data_compressor(input_file_t *f, void *obj)
{
	pkg_desc_t *desc = obj;

	desc->datacmp = compressor_by_name(f->line);

	if (desc->datacmp == NULL) {
		input_file_complain(f, "unkown compressor");
		return -1;
	}

	return 0;
}

static const keyword_handler_t line_hooks[] = {
	{ "toc-compressor", handle_toc_compressor },
	{ "data-compressor", handle_data_compressor },
	{ "requires", handle_requires },
	{ "name", handle_name },
};

#define NUM_LINE_HOOKS (sizeof(line_hooks) / sizeof(line_hooks[0]))

static compressor_t *get_default_compressor(void)
{
	compressor_t *cmp;

	cmp = compressor_by_id(PKG_COMPRESSION_LZMA);
	if (cmp != NULL)
		return cmp;

	cmp = compressor_by_id(PKG_COMPRESSION_ZLIB);
	if (cmp != NULL)
		return cmp;

	cmp = compressor_by_id(PKG_COMPRESSION_NONE);
	if (cmp != NULL)
		return cmp;

	return cmp;
}

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

	if (desc->datacmp == NULL)
		desc->datacmp = get_default_compressor();

	if (desc->toccmp == NULL)
		desc->toccmp = get_default_compressor();

	if (desc->datacmp == NULL || desc->toccmp == NULL) {
		fputs("no compressor implementations available\n", stderr);
		desc_free(desc);
		return -1;
	}

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
