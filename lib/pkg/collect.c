/* SPDX-License-Identifier: ISC */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "pkg/pkgreader.h"
#include "pkg/pkglist.h"

int collect_dependencies(int repofd, struct pkg_dep_list *list)
{
	struct pkg_dep_node *it;
	pkg_dependency_t dep;
	uint8_t buffer[257];
	pkg_reader_t *rd;
	pkg_header_t hdr;
	size_t i;
	int ret;

	for (it = list->head; it != NULL; it = it->next) {
		rd = pkg_reader_open_repo(repofd, it->name);
		if (rd == NULL)
			return -1;

		ret = pkg_reader_read_payload(rd, &hdr, sizeof(hdr));
		if (ret < 0)
			goto fail;
		if ((size_t)ret < sizeof(hdr))
			goto fail_trunc;

		it->num_deps = le16toh(hdr.num_depends);
		it->deps = calloc(sizeof(it->deps[0]), it->num_deps);
		if (it->deps == NULL)
			goto fail_oom;

		for (i = 0; i < it->num_deps; ++i) {
			ret = pkg_reader_read_payload(rd, &dep, sizeof(dep));
			if (ret < 0)
				goto fail;
			if ((size_t)ret < sizeof(hdr))
				goto fail_trunc;

			ret = pkg_reader_read_payload(rd, buffer,
						      dep.name_length);
			if (ret < 0)
				goto fail;
			if ((size_t)ret < dep.name_length)
				goto fail_trunc;

			buffer[dep.name_length] = '\0';

			if (strcmp((char *)buffer, it->name) == 0) {
				fprintf(stderr,
					"%s: package depends on itself\n",
					it->name);
				goto fail;
			}

			it->deps[i] = find_pkg(list, (char *)buffer);
			if (it->deps[i] == NULL) {
				it->deps[i] = append_pkg(list, (char *)buffer);
				if (it->deps[i] == NULL)
					goto fail;
			}
		}

		pkg_reader_close(rd);
	}

	return 0;
fail_trunc:
	fprintf(stderr, "%s: truncated header record\n",
		pkg_reader_get_filename(rd));
	goto fail;
fail_oom:
	fputs("out of memory\n", stderr);
	goto fail;
fail:
	pkg_reader_close(rd);
	return -1;
}
