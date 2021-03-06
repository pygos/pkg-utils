/* SPDX-License-Identifier: ISC */
#ifndef DUMP_H
#define DUMP_H

#include <sys/stat.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "util/util.h"

#include "filelist/image_entry.h"

#include "pkg/pkgformat.h"
#include "pkg/pkgreader.h"
#include "pkg/pkgio.h"

#include "command.h"

typedef enum {
	DUMP_TOC = 0x01,
	DUMP_DEPS = 0x02,
	DUMP_ALL = (DUMP_TOC | DUMP_DEPS),
} DUMP_FLAGS;

int dump_header(pkg_reader_t *pkg, int flags);

#endif /* DUMP_H */
