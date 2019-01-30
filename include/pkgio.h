#ifndef PKGIO_H
#define PKGIO_H

#include "pkgreader.h"
#include "image_entry.h"

image_entry_t *image_entry_list_from_package(pkg_reader_t *pkg);

#endif /* PKGIO_H */
