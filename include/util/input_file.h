/* SPDX-License-Identifier: ISC */
#ifndef INPUT_FILE_H
#define INPUT_FILE_H

#include <stddef.h>

typedef struct {
	const char *name;
	int (*handle)(char *line, const char *filename,
		      size_t linenum, void *obj);
} keyword_handler_t;

int process_file(const char *filename, const keyword_handler_t *handlers,
		 size_t count, void *obj);

void input_file_complain(const char *filename, size_t linenum,
			 const char *msg);

#endif /* INPUT_FILE_H */
