/* SPDX-License-Identifier: ISC */
#ifndef BUILDSTRATEGY_H
#define BUILDSTRATEGY_H

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

#include "command.h"
#include "util/util.h"
#include "util/hashtable.h"

enum {
	FLAG_BUILD_PKG = 0x01,
};

typedef struct source_pkg_t {
	struct source_pkg_t *next;
	char *name;

	struct source_pkg_t **depends;
	size_t num_depends;

	int flags;
} source_pkg_t;

int src_pkg_init(void);

void src_pkg_cleanup(void);

source_pkg_t *src_pkg_get(const char *name);

int src_pkg_output_build_order(void);

int provider_init(void);

void provider_cleanup(void);

int provider_add(const char *sourcepkg, const char *binpkg);

int provider_add_prefered(const char *binpkg, const char *sourcepkg);

source_pkg_t *provider_get(const char *parent, const char *binpkg);

#endif /* BUILDSTRATEGY_H */
