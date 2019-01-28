#include "dump.h"

static const char *dependency_type[] = {
	[PKG_DEPENDENCY_REQUIRES] = "requires",
};

int dump_header(pkg_reader_t *pkg, int flags)
{
	pkg_dependency_t dep;
	uint8_t buffer[257];
	pkg_header_t hdr;
	ssize_t ret;
	uint16_t i;

	ret = pkg_reader_read_payload(pkg, &hdr, sizeof(hdr));
	if (ret < 0)
		return -1;
	if ((size_t)ret < sizeof(hdr))
		goto fail_trunc;

	hdr.num_depends = le16toh(hdr.num_depends);

	if (flags & DUMP_DEPS) {
		printf("Dependencies:\n");

		for (i = 0; i < hdr.num_depends; ++i) {
			ret = pkg_reader_read_payload(pkg, &dep, sizeof(dep));
			if (ret < 0)
				return -1;
			if ((size_t)ret < sizeof(hdr))
				goto fail_trunc;

			ret = pkg_reader_read_payload(pkg, buffer,
						      dep.name_length);
			if (ret < 0)
				return -1;
			if ((size_t)ret < dep.name_length)
				goto fail_trunc;

			buffer[dep.name_length] = '\0';

			printf("\t%s: %s\n", dependency_type[dep.type],
			       (char *)buffer);
		}

		fputc('\n', stdout);
	}

	return 0;
fail_trunc:
	fprintf(stderr, "%s: truncated header record\n",
		pkg_reader_get_filename(pkg));
	return -1;
}
