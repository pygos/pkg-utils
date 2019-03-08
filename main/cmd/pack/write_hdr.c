/* SPDX-License-Identifier: ISC */
#include "pack.h"

int write_header_data(pkg_writer_t *wr, pkg_desc_t *desc)
{
	pkg_dependency_t pkgdep;
	size_t dep_count = 0;
	dependency_t *dep;
	pkg_header_t hdr;

	for (dep = desc->deps; dep != NULL; dep = dep->next)
		dep_count += 1;

	if (dep_count > 0xFFFF) {
		fputs("too many package dependencies\n", stderr);
		return -1;
	}

	memset(&hdr, 0, sizeof(hdr));
	hdr.num_depends = htole16(dep_count);

	if (pkg_writer_write_payload(wr, &hdr, sizeof(hdr)))
		return -1;

	for (dep = desc->deps; dep != NULL; dep = dep->next) {
		memset(&pkgdep, 0, sizeof(pkgdep));

		pkgdep.type = dep->type;
		pkgdep.name_length = strlen(dep->name);

		if (pkg_writer_write_payload(wr, &pkgdep, sizeof(pkgdep)))
			return -1;

		if (pkg_writer_write_payload(wr, dep->name,
					     pkgdep.name_length)) {
			return -1;
		}
	}

	return pkg_writer_end_record(wr);
}
