#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

#include "util/util.h"

int foreach_line_in_file(const char *filename, void *usr, linecb_t fun)
{
	FILE *f = fopen(filename, "r");
	char *line = NULL, *ptr;
	size_t n, lineno = 1;
	ssize_t ret;

	if (f == NULL) {
		perror(filename);
		return -1;
	}

	for (;;) {
		free(line);

		line = NULL;
		errno = 0;
		n = 0;
		ret = getline(&line, &n, f);

		if (ret < 0) {
			if (errno == 0) {
				ret = 0;
				break;
			}
			perror(filename);
			break;
		}

		for (ptr = line; isspace(*ptr); ++ptr)
			;

		if (ptr != line)
			memmove(line, ptr, strlen(ptr) + 1);

		ptr = line + strlen(line);
		while (ptr > line && isspace(ptr[-1]))
			--ptr;
		*ptr = '\0';

		if (*line == '\0')
			continue;

		if (fun(usr, filename, lineno++, line)) {
			ret = -1;
			break;
		}
	}

	free(line);
	fclose(f);
	return ret;
}
