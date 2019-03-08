/* SPDX-License-Identifier: ISC */
#ifndef PACK_H
#define PACK_H

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>

#include "util/input_file.h"
#include "util/util.h"

#include "filelist/image_entry.h"

#include "compressor.h"
#include "pkgformat.h"
#include "pkgwriter.h"
#include "command.h"

typedef struct dependency_t {
	struct dependency_t *next;
	int type;
	char name[];
} dependency_t;

typedef struct {
	compressor_t *datacmp;
	compressor_t *toccmp;
	dependency_t *deps;
	char *name;
} pkg_desc_t;

int filelist_mkdir(char *line, const char *filename,
		   size_t linenum, void *obj);

int filelist_mkslink(char *line, const char *filename,
		     size_t linenum, void *obj);

int filelist_mkfile(char *line, const char *filename,
		    size_t linenum, void *obj);

int filelist_read(const char *filename, image_entry_t **out);

int write_toc(pkg_writer_t *wr, image_entry_t *list, compressor_t *cmp);

int write_files(pkg_writer_t *wr, image_entry_t *list, compressor_t *cmp);

int desc_read(const char *path, pkg_desc_t *desc);

void desc_free(pkg_desc_t *desc);

int write_header_data(pkg_writer_t *wr, pkg_desc_t *desc);

#endif /* PACK_H */
