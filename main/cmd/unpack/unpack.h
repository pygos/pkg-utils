#ifndef UNPACK_H
#define UNPACK_H

#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "image_entry.h"
#include "pkgreader.h"
#include "command.h"
#include "pkgio.h"
#include "util.h"

enum {
	FLAG_NO_CHOWN = 0x01,
	FLAG_NO_CHMOD = 0x02,
};

int mkdir_p(const char *path);

int pkg_unpack(int rootfd, int flags, pkg_reader_t *rd);

#endif /* UNPACK_H */
