/* SPDX-License-Identifier: ISC */
#ifndef PKGIO_H
#define PKGIO_H

#include "pkgreader.h"
#include "filelist/image_entry.h"

enum {
	UNPACK_NO_CHOWN = 0x01,
	UNPACK_NO_CHMOD = 0x02,
	UNPACK_NO_SYMLINKS = 0x04,
	UNPACK_NO_DEVICES = 0x08,
};

int pkg_unpack(int rootfd, int flags, pkg_reader_t *rd);

int image_entry_list_from_package(pkg_reader_t *pkg, image_entry_t ** list);

#endif /* PKGIO_H */
