/* SPDX-License-Identifier: ISC */
#ifndef UTIL_H
#define UTIL_H

#include <sys/types.h>

int canonicalize_name(char *filename);

ssize_t write_retry(int fd, void *data, size_t size);

ssize_t read_retry(int fd, void *buffer, size_t size);

int mkdir_p(const char *path);

typedef int (*linecb_t)(void *usr, const char *filename,
			size_t linenum, char *line);

int foreach_line_in_file(const char *filename, void *usr, linecb_t fun);

#endif /* UTIL_H */
