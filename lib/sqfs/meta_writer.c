/* SPDX-License-Identifier: ISC */
#include "sqfs/squashfs.h"
#include "util/util.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

meta_writer_t *meta_writer_create(int fd, compressor_stream_t *strm)
{
	meta_writer_t *m = calloc(1, sizeof(*m));

	if (m == NULL) {
		perror("creating meta data writer");
		return NULL;
	}

	m->strm = strm;
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

	if (m->offset == 0)
		return 0;

	ret = m->strm->do_block(m->strm, m->data + 2, m->scratch,
				m->offset, sizeof(m->scratch));
	if (ret < 0)
		return -1;

	if (ret > 0 && (size_t)ret < m->offset) {
		memcpy(m->data + 2, m->scratch, ret);
		((uint16_t *)m->data)[0] = htole16(ret);
		count = ret + 2;
	} else {
		((uint16_t *)m->data)[0] = htole16(m->offset | 0x8000);
		count = m->offset + 2;
	}

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
