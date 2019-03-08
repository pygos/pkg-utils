/* SPDX-License-Identifier: ISC */
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "util/input_file.h"
#include "util/util.h"

void input_file_complain(const char *filename, size_t linenum, const char *msg)
{
	fprintf(stderr, "%s: %zu: %s\n", filename, linenum, msg);
}

struct userdata {
	const keyword_handler_t *handlers;
	size_t count;
	void *obj;
};

static int handle_line(void *usr, const char *filename,
		       size_t linenum, char *line)
{
	struct userdata *u = usr;
	size_t i, len;

	while (isspace(*line))
		++line;

	if (*line == '\0' || *line == '#')
		return 0;

	for (i = 0; i < u->count; ++i) {
		len = strlen(u->handlers[i].name);

		if (strncmp(line, u->handlers[i].name, len) != 0)
			continue;
		if (!isspace(line[len]) && line[len] != '\0')
			continue;
		for (line += len; isspace(*line); ++line)
			;
		break;
	}

	if (i == u->count) {
		fprintf(stderr, "%s: %zu: unknown keyword\n",
			filename, linenum);
		return -1;
	}

	return u->handlers[i].handle(line, filename, linenum, u->obj);
}

int process_file(const char *filename, const keyword_handler_t *handlers,
		 size_t count, void *obj)
{
	struct userdata u = { handlers, count, obj };

	return foreach_line_in_file(filename, &u, handle_line);
}
