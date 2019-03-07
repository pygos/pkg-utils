#ifndef BUILDSTRATEGY_H
#define BUILDSTRATEGY_H

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

#include "command.h"
#include "util/hashtable.h"

enum {
	FLAG_BUILD_PKG = 0x01,
};

typedef struct source_pkg_t {
	char *name;

	struct source_pkg_t **depends;
	size_t num_depends;

	int flags;
} source_pkg_t;

#endif /* BUILDSTRATEGY_H */
