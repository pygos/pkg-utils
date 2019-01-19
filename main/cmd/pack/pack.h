#ifndef PACK_H
#define PACK_H

#include <sys/types.h>
#include <sys/stat.h>
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

#include "image_entry.h"
#include "compressor.h"
#include "pkgformat.h"
#include "pkgwriter.h"
#include "command.h"
#include "util.h"

typedef struct {
	FILE *f;
	char *line;
	const char *filename;
	size_t linenum;
} input_file_t;

image_entry_t *filelist_mkdir(input_file_t *f);

image_entry_t *filelist_mkslink(input_file_t *f);

image_entry_t *filelist_mkfile(input_file_t *f);

image_entry_t *filelist_read(const char *filename);

int write_toc(pkg_writer_t *wr, image_entry_t *list, compressor_t *cmp);

int write_files(pkg_writer_t *wr, image_entry_t *list, compressor_t *cmp);

#endif /* PACK_H */
