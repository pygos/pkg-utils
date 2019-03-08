#ifndef INPUT_FILE_H
#define INPUT_FILE_H

#include <stdio.h>

typedef struct {
	FILE *f;
	char *line;
	const char *filename;
	size_t linenum;
} input_file_t;

typedef struct {
	const char *name;
	int (*handle)(input_file_t *f, void *obj);
} keyword_handler_t;

int process_file(const char *filename, const keyword_handler_t *handlers,
		 size_t count, void *obj);

void input_file_complain(const input_file_t *f, const char *msg);

#endif /* INPUT_FILE_H */
