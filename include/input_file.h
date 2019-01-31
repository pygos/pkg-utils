#ifndef INPUT_FILE_H
#define INPUT_FILE_H

#include <stdio.h>

typedef struct {
	FILE *f;
	char *line;
	const char *filename;
	size_t linenum;
} input_file_t;

int prefetch_line(input_file_t *f);

void cleanup_file(input_file_t *f);

int open_file(input_file_t *f, const char *filename);

#endif /* INPUT_FILE_H */
