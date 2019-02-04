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

void cleanup_file(input_file_t *f);

int open_file(input_file_t *f, const char *filename);

int process_file(input_file_t *f, const keyword_handler_t *handlers,
		 size_t count, void *obj);

void input_file_complain(const input_file_t *f, const char *msg);

#endif /* INPUT_FILE_H */
