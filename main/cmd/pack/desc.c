#include "pack.h"

static int handle_requires(char *line, const char *filename,
			   size_t linenum, void *obj)
{
	char *ptr = line, *end;
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
			input_file_complain(filename, linenum, "dependency name too long");
			return -1;
		}

		dep = calloc(1, sizeof(*dep) + len + 1);
		if (dep == NULL) {
			input_file_complain(filename, linenum,
					    "out of memory");
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

static int handle_toc_compressor(char *line, const char *filename,
				 size_t linenum, void *obj)
{
	pkg_desc_t *desc = obj;

	desc->toccmp = compressor_by_name(line);

	if (desc->toccmp == NULL) {
		input_file_complain(filename, linenum, "unkown compressor");
		return -1;
	}

	return 0;
}

static int handle_data_compressor(char *line, const char *filename,
				  size_t linenum, void *obj)
{
	pkg_desc_t *desc = obj;

	desc->datacmp = compressor_by_name(line);

	if (desc->datacmp == NULL) {
		input_file_complain(filename, linenum, "unkown compressor");
		return -1;
	}

	return 0;
}

static const keyword_handler_t line_hooks[] = {
	{ "toc-compressor", handle_toc_compressor },
	{ "data-compressor", handle_data_compressor },
	{ "requires", handle_requires },
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
	char *ptr;

	memset(desc, 0, sizeof(*desc));

	if (process_file(path, line_hooks, NUM_LINE_HOOKS, desc))
		return -1;

	if (desc->datacmp == NULL)
		desc->datacmp = get_default_compressor();

	if (desc->toccmp == NULL)
		desc->toccmp = get_default_compressor();

	if (desc->datacmp == NULL || desc->toccmp == NULL) {
		fputs("no compressor implementations available\n", stderr);
		desc_free(desc);
		return -1;
	}

	ptr = strrchr(path, '/');

	desc->name = strdup((ptr == NULL) ? path : (ptr + 1));
	if (desc->name == NULL) {
		fputs("out of memory\n", stderr);
		desc_free(desc);
		return -1;
	}

	ptr = strrchr(desc->name, '.');
	if (ptr != NULL)
		*ptr = '\0';

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
