#ifndef DUMP_H
#define DUMP_H

#include <sys/stat.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "image_entry.h"
#include "pkgformat.h"
#include "pkgreader.h"
#include "command.h"
#include "util.h"

typedef enum {
	TOC_FORMAT_PRETTY = 0,
	TOC_FORMAT_SQFS = 1,
	TOC_FORMAT_INITRD = 2,
} TOC_FORMAT;

typedef enum {
	DUMP_TOC = 0x01,
	DUMP_DEPS = 0x02,
	DUMP_ALL = (DUMP_TOC | DUMP_DEPS),
} DUMP_FLAGS;

int dump_toc(image_entry_t *list, const char *root, TOC_FORMAT format);

int dump_header(pkg_reader_t *pkg, int flags);

#endif /* DUMP_H */
