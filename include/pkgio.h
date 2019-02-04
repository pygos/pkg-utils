#ifndef PKGIO_H
#define PKGIO_H

#include "pkgreader.h"
#include "filelist/image_entry.h"

enum {
	UNPACK_NO_CHOWN = 0x01,
	UNPACK_NO_CHMOD = 0x02,
	UNPACK_NO_SYMLINKS = 0x04,
};

int pkg_unpack(int rootfd, int flags, pkg_reader_t *rd);

image_entry_t *image_entry_list_from_package(pkg_reader_t *pkg);

#endif /* PKGIO_H */
