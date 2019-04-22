/* SPDX-License-Identifier: ISC */
#include <endian.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "sqfs/squashfs.h"
#include "util/util.h"

int sqfs_super_init(sqfs_super_t *s, int64_t timestamp,
		    uint32_t blocksize, E_SQFS_COMPRESSOR comp)
{
	memset(s, 0, sizeof(*s));

	if (blocksize & (blocksize - 1)) {
		fputs("Block size must be a power of 2\n", stderr);
		return -1;
	}

	if (blocksize < 4096 || blocksize >= (1 << 24)) {
		fputs("Block size must be between 4k and 1M\n", stderr);
		return -1;
	}

	if (timestamp < 0 || timestamp > 0x00000000FFFFFFFF) {
		fputs("Timestamp too large for squashfs\n", stderr);
		return -1;
	}

	s->magic = SQFS_MAGIC;
	s->modification_time = timestamp;
	s->block_size = blocksize;
	s->compression_id = comp;
	s->flags = SQFS_FLAG_NO_FRAGMENTS | SQFS_FLAG_NO_XATTRS;
	s->version_major = SQFS_VERSION_MAJOR;
	s->version_minor = SQFS_VERSION_MINOR;
	s->bytes_used = sizeof(*s);
	s->id_table_start = 0xFFFFFFFFFFFFFFFFUL;
	s->xattr_id_table_start = 0xFFFFFFFFFFFFFFFFUL;
	s->inode_table_start = 0xFFFFFFFFFFFFFFFFUL;
	s->directory_table_start = 0xFFFFFFFFFFFFFFFFUL;
	s->fragment_table_start = 0xFFFFFFFFFFFFFFFFUL;
	s->export_table_start = 0xFFFFFFFFFFFFFFFFUL;

	while (blocksize != 0) {
		s->block_log += 1;
		blocksize >>= 1;
	}

	s->block_log -= 1;
	return 0;
}

int sqfs_super_write(const sqfs_super_t *super, int outfd)
{
	sqfs_super_t copy;
	ssize_t ret;
	off_t off;

	copy.magic = htole32(super->magic);
	copy.inode_count = htole32(super->inode_count);
	copy.modification_time = htole32(super->modification_time);
	copy.block_size = htole32(super->block_size);
	copy.fragment_entry_count = htole32(super->fragment_entry_count);
	copy.compression_id = htole16(super->compression_id);
	copy.block_log = htole16(super->block_log);
	copy.flags = htole16(super->flags);
	copy.id_count = htole16(super->id_count);
	copy.version_major = htole16(super->version_major);
	copy.version_minor = htole16(super->version_minor);
	copy.root_inode_ref = htole64(super->root_inode_ref);
	copy.bytes_used = htole64(super->bytes_used);
	copy.id_table_start = htole64(super->id_table_start);
	copy.xattr_id_table_start = htole64(super->xattr_id_table_start);
	copy.inode_table_start = htole64(super->inode_table_start);
	copy.directory_table_start = htole64(super->directory_table_start);
	copy.fragment_table_start = htole64(super->fragment_table_start);
	copy.export_table_start = htole64(super->export_table_start);

	off = lseek(outfd, 0, SEEK_SET);
	if (off == (off_t)-1) {
		perror("squashfs writing super block: seek on output file");
		return -1;
	}

	ret = write_retry(outfd, &copy, sizeof(copy));

	if (ret < 0) {
		perror("squashfs writing super block");
		return -1;
	}

	if (ret == 0) {
		fputs("squashfs writing super block: truncated write\n",
		      stderr);
		return -1;
	}

	return 0;
}

compressor_stream_t *sqfs_get_compressor(sqfs_super_t *s)
{
	PKG_COMPRESSION id;
	compressor_t *cmp;
	size_t xz_dictsz;
	void *options;

	switch (s->compression_id) {
	case SQFS_COMP_GZIP:
		id = PKG_COMPRESSION_ZLIB;
		options = NULL;
		break;
	case SQFS_COMP_XZ:
		id = PKG_COMPRESSION_LZMA;
		xz_dictsz = s->block_size;
		options = &xz_dictsz;
		break;
	default:
		fputs("unsupported compressor\n", stderr);
		return NULL;
	}

	cmp = compressor_by_id(id);
	if (cmp == NULL)
		return NULL;

	return cmp->compression_stream(cmp, options);
}
