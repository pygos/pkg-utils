#include "pack.h"

int prefetch_line(input_file_t *f)
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
	return 0;
}

void cleanup_file(input_file_t *f)
{
	fclose(f->f);
	free(f->line);
}

int open_file(input_file_t *f, const char *filename)
{
	memset(f, 0, sizeof(*f));

	f->filename = filename;
	f->f = fopen(filename, "r");

	if (f->f == NULL) {
		perror(f->filename);
		return -1;
	}

	return 0;
}
