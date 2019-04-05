/* SPDX-License-Identifier: ISC */
#ifndef PKGWRITER_H
#define PKGWRITER_H

#include <stdbool.h>

#include "comp/compressor.h"
#include "pkgformat.h"

typedef struct pkg_writer_t pkg_writer_t;

pkg_writer_t *pkg_writer_open(const char *path, bool force);

void pkg_writer_close(pkg_writer_t *writer);

int pkg_writer_start_record(pkg_writer_t *writer, uint32_t magic,
			    compressor_t *cmp);

int pkg_writer_write_payload(pkg_writer_t *wr, void *data, size_t size);

int pkg_writer_end_record(pkg_writer_t *wr);

#endif /* PKGWRITER_H */
