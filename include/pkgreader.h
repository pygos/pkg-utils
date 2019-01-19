#ifndef PKGREADER_H
#define PKGREADER_H

#include "pkgformat.h"

typedef struct pkg_reader_t pkg_reader_t;

pkg_reader_t *pkg_reader_open(const char *path);

void pkg_reader_close(pkg_reader_t *reader);

int pkg_reader_get_next_record(pkg_reader_t *reader);

record_t *pkg_reader_current_record_header(pkg_reader_t *reader);

ssize_t pkg_reader_read_payload(pkg_reader_t *reader, void *buffer,
				size_t size);

int pkg_reader_rewind(pkg_reader_t *reader);

const char *pkg_reader_get_filename(pkg_reader_t *reader);

#endif /* PKGREADER_H */
