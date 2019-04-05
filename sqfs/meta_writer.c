/* SPDX-License-Identifier: ISC */
#include "pkg2sqfs.h"

meta_writer_t *meta_writer_create(int fd)
{
	meta_writer_t *m = calloc(1, sizeof(*m));

	if (m == NULL) {
		perror("creating meta data writer");
		return NULL;
	}

	m->outfd = fd;
	return m;
}

void meta_writer_destroy(meta_writer_t *m)
{
	free(m);
}

int meta_writer_flush(meta_writer_t *m)
{
	ssize_t ret, count;

	/* TODO: compress buffer */

	if (m->offset == 0)
		return 0;

	((uint16_t *)m->data)[0] = htole16(m->offset | 0x8000);
	count = m->offset + 2;

	ret = write_retry(m->outfd, m->data, count);
	if (ret < 0) {
		perror("writing meta data");
		return -1;
	}

	if (ret < count) {
		fputs("meta data was truncated\n", stderr);
		return -1;
	}

	memset(m->data, 0, sizeof(m->data));
	m->offset = 0;
	m->written += count;
	return 0;
}

int meta_writer_append(meta_writer_t *m, const void *data, size_t size)
{
	size_t diff;

	while (size != 0) {
		diff = sizeof(m->data) - 2 - m->offset;

		if (diff == 0) {
			if (meta_writer_flush(m))
				return -1;
			diff = sizeof(m->data) - 2;
		}

		if (diff > size)
			diff = size;

		memcpy(m->data + 2 + m->offset, data, diff);
		m->offset += diff;
		size -= diff;
		data = (const char *)data + diff;
	}

	return 0;
}
