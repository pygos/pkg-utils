/* SPDX-License-Identifier: ISC */
#include "pkg2sqfs.h"

int sqfs_write_table(sqfs_info_t *info, const void *data, size_t entsize,
		     size_t count, uint64_t *startblock)
{
	size_t ent_per_blocks = SQFS_META_BLOCK_SIZE / entsize;
	uint64_t blocks[count / ent_per_blocks + 1];
	size_t i, blkidx = 0, tblsize;
	meta_writer_t *m;
	ssize_t ret;

	/* Write actual data. Whenever we cross a block boundary, remember
	   the block start offset */
	m = meta_writer_create(info->outfd);
	if (m == NULL)
		return -1;

	for (i = 0; i < count; ++i) {
		if (blkidx == 0 || m->written > blocks[blkidx - 1])
			blocks[blkidx++] = m->written;

		if (meta_writer_append(m, data, entsize))
			goto fail;

		data = (const char *)data + entsize;
	}

	if (meta_writer_flush(m))
		goto fail;

	for (i = 0; i < blkidx; ++i)
		blocks[i] = htole64(blocks[i] + info->super.bytes_used);

	info->super.bytes_used += m->written;
	meta_writer_destroy(m);

	/* write new index table */
	*startblock = info->super.bytes_used;
	tblsize = sizeof(blocks[0]) * blkidx;

	ret = write_retry(info->outfd, blocks, tblsize);
	if (ret < 0) {
		perror("writing index table");
		return -1;
	}

	if ((size_t)ret < tblsize) {
		fputs("index table truncated\n", stderr);
		return -1;
	}

	info->super.bytes_used += tblsize;
	return 0;
fail:
	meta_writer_destroy(m);
	return -1;
}
