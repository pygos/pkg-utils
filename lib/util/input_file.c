#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "util/input_file.h"

static int prefetch_line(input_file_t *f)
{
	char *line, *ptr;
	ssize_t ret;
	size_t n;
retry:
	free(f->line);
	f->line = NULL;

	errno = 0;
	line = NULL;
	n = 0;
	ret = getline(&line, &n, f->f);

	if (ret < 0) {
		if (errno != 0) {
			perror(f->filename);
			free(line);
			return -1;
		}
		free(line);
		return 1;
	}

	n = strlen(line);
	while (n >0 && isspace(line[n - 1]))
		--n;
	line[n] = '\0';

	f->line = line;
	f->linenum += 1;

	for (ptr = f->line; isspace(*ptr); ++ptr)
		;

	if (*ptr == '\0' || *ptr == '#')
		goto retry;

	if (ptr != f->line)
		memmove(f->line, ptr, strlen(ptr) + 1);

	ptr = f->line + strlen(f->line);

	while (ptr > f->line && isspace(ptr[-1]))
		--ptr;
	*ptr = '\0';

	if (f->line[0] == '\0')
		goto retry;
	return 0;
}

void input_file_complain(const input_file_t *f, const char *msg)
{
	fprintf(stderr, "%s: %zu: %s\n", f->filename, f->linenum, msg);
}

int process_file(const char *filename, const keyword_handler_t *handlers,
		 size_t count, void *obj)
{
	input_file_t f;
	size_t i, len;
	char *ptr;
	int ret;

	memset(&f, 0, sizeof(f));

	f.filename = filename;
	f.f = fopen(filename, "r");

	if (f.f == NULL) {
		perror(f.filename);
		return -1;
	}

	for (;;) {
		ret = prefetch_line(&f);
		if (ret < 0)
			goto fail;
		if (ret > 0)
			break;

		ptr = f.line;

		for (i = 0; i < count; ++i) {
			len = strlen(handlers[i].name);

			if (strncmp(ptr, handlers[i].name, len) != 0)
				continue;
			if (!isspace(ptr[len]) && ptr[len] != '\0')
				continue;
			for (ptr += len; isspace(*ptr); ++ptr)
				;
			memmove(f.line, ptr, strlen(ptr) + 1);
			break;
		}

		if (i == count) {
			fprintf(stderr, "%s: %zu: unknown keyword\n",
				f.filename, f.linenum);
			goto fail;
		}

		if (handlers[i].handle(&f, obj))
			goto fail;
	}

	fclose(f.f);
	free(f.line);
	return 0;
fail:
	fclose(f.f);
	free(f.line);
	return -1;
}
