/* SPDX-License-Identifier: ISC */
#include "pkg2sqfs.h"

static int write_block(node_t *node, sqfs_info_t *info,
		       compressor_stream_t *strm)
{
	size_t idx, bs;
	ssize_t ret;
	void *ptr;

	idx = info->file_block_count++;
	bs = info->super.block_size;

	ret = strm->do_block(strm, info->block, info->scratch, bs, bs);
	if (ret < 0)
		return -1;

	if (ret > 0) {
		ptr = info->scratch;
		bs = ret;
		node->data.file->blocksizes[idx] = bs;
	} else {
		ptr = info->block;
		node->data.file->blocksizes[idx] = bs | (1 << 24);
	}

	ret = write_retry(info->outfd, ptr, bs);
	if (ret < 0) {
		perror("writing to output file");
		return -1;
	}

	if ((size_t)ret < bs) {
		fputs("write to output file truncated\n", stderr);
		return -1;
	}

	info->super.bytes_used += bs;
	return 0;
}

static int flush_fragments(sqfs_info_t *info, compressor_stream_t *strm)
{
	size_t newsz, size;
	file_info_t *fi;
	uint64_t offset;
	void *new, *ptr;
	ssize_t ret;

	if (info->num_fragments == info->max_fragments) {
		newsz = info->max_fragments ? info->max_fragments * 2 : 16;
		new = realloc(info->fragments,
			      sizeof(info->fragments[0]) * newsz);

		if (new == NULL) {
			perror("appending to fragment table");
			return -1;
		}

		info->max_fragments = newsz;
		info->fragments = new;
	}

	offset = info->super.bytes_used;
	size = info->frag_offset;

	for (fi = info->frag_list; fi != NULL; fi = fi->frag_next)
		fi->fragment = info->num_fragments;

	ret = strm->do_block(strm, info->fragment, info->scratch,
			     size, info->super.block_size);
	if (ret < 0)
		return -1;

	info->fragments[info->num_fragments].start_offset = htole64(offset);
	info->fragments[info->num_fragments].pad0 = 0;

	if (ret > 0 && (size_t)ret < size) {
		size = ret;
		info->fragments[info->num_fragments].size = htole32(size);
		ptr = info->scratch;
	} else {
		info->fragments[info->num_fragments].size =
			htole32(size | (1 << 24));
		ptr = info->fragment;
	}

	info->num_fragments += 1;

	ret = write_retry(info->outfd, ptr, size);
	if (ret < 0) {
		perror("writing to output file");
		return -1;
	}

	if ((size_t)ret < size) {
		fputs("write to output file truncated\n", stderr);
		return -1;
	}

	memset(info->fragment, 0, info->super.block_size);

	info->super.bytes_used += size;
	info->frag_offset = 0;
	info->frag_list = NULL;

	info->super.flags &= ~SQFS_FLAG_NO_FRAGMENTS;
	info->super.flags |= SQFS_FLAG_ALWAYS_FRAGMENTS;
	return 0;
}

static int add_fragment(file_info_t *fi, sqfs_info_t *info, size_t size,
			compressor_stream_t *strm)
{
	if (info->frag_offset + size > info->super.block_size) {
		if (flush_fragments(info, strm))
			return -1;
	}

	fi->fragment_offset = info->frag_offset;
	fi->frag_next = info->frag_list;
	info->frag_list = fi;

	memcpy((char *)info->fragment + info->frag_offset, info->block, size);
	info->frag_offset += size;
	return 0;
}

static int process_file(node_t *node, sqfs_info_t *info,
			compressor_stream_t *strm)
{
	uint64_t count = node->data.file->size;
	int ret;
	size_t diff;

	node->data.file->startblock = info->super.bytes_used;
	info->file_block_count = 0;

	while (count != 0) {
		diff = count > (uint64_t)info->super.block_size ?
			info->super.block_size : count;

		ret = pkg_reader_read_payload(info->rd, info->block, diff);
		if (ret < 0)
			return -1;
		if ((size_t)ret < diff)
			goto fail_trunc;

		if (diff < info->super.block_size) {
			if (add_fragment(node->data.file, info, diff, strm))
				return -1;
		} else {
			if (write_block(node, info, strm))
				return -1;
		}

		count -= diff;
	}

	return 0;
fail_trunc:
	fprintf(stderr, "%s: truncated data block\n",
		pkg_reader_get_filename(info->rd));
	return -1;
}

int pkg_data_to_sqfs(sqfs_info_t *info, compressor_stream_t *strm)
{
	file_data_t frec;
	record_t *hdr;
	node_t *node;
	int ret;

	if (pkg_reader_rewind(info->rd))
		return -1;

	memset(info->fragment, 0, info->super.block_size);

	for (;;) {
		ret = pkg_reader_get_next_record(info->rd);
		if (ret == 0)
			break;
		if (ret < 0)
			return -1;

		hdr = pkg_reader_current_record_header(info->rd);
		if (hdr->magic != PKG_MAGIC_DATA)
			continue;

		for (;;) {
			ret = pkg_reader_read_payload(info->rd, &frec,
						      sizeof(frec));
			if (ret == 0)
				break;
			if (ret < 0)
				return -1;
			if ((size_t)ret < sizeof(frec))
				goto fail_trunc;

			frec.id = le32toh(frec.id);

			node = vfs_node_from_file_id(&info->fs, frec.id);
			if (node == NULL)
				goto fail_meta;

			if (process_file(node, info, strm))
				return -1;
		}
	}

	if (info->frag_offset != 0) {
		if (flush_fragments(info, strm))
			return -1;
	}

	return 0;
fail_meta:
	fprintf(stderr, "%s: missing meta information for file %u\n",
		pkg_reader_get_filename(info->rd), (unsigned int)frec.id);
	return -1;
fail_trunc:
	fprintf(stderr, "%s: truncated file data record\n",
		pkg_reader_get_filename(info->rd));
	return -1;
}
