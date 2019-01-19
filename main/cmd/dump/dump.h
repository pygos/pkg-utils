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

int dump_toc(image_entry_t *list, const char *root, TOC_FORMAT format);

#endif /* DUMP_H */
