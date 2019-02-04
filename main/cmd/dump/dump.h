#ifndef DUMP_H
#define DUMP_H

#include <sys/stat.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "util/util.h"

#include "image_entry.h"
#include "pkgformat.h"
#include "pkgreader.h"
#include "command.h"
#include "pkgio.h"

typedef enum {
	DUMP_TOC = 0x01,
	DUMP_DEPS = 0x02,
	DUMP_ALL = (DUMP_TOC | DUMP_DEPS),
} DUMP_FLAGS;

int dump_header(pkg_reader_t *pkg, int flags);

#endif /* DUMP_H */
