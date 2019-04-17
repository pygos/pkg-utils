/* SPDX-License-Identifier: ISC */
#include <endian.h>
#include <string.h>
#include <stdio.h>

#include "util/util.h"
#include "pkg2sqfs.h"

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

int sqfs_super_write(sqfs_info_t *info)
{
	sqfs_super_t copy;
	ssize_t ret;
	off_t off;

	copy.magic = htole32(info->super.magic);
	copy.inode_count = htole32(info->super.inode_count);
	copy.modification_time = htole32(info->super.modification_time);
	copy.block_size = htole32(info->super.block_size);
	copy.fragment_entry_count = htole32(info->super.fragment_entry_count);
	copy.compression_id = htole16(info->super.compression_id);
	copy.block_log = htole16(info->super.block_log);
	copy.flags = htole16(info->super.flags);
	copy.id_count = htole16(info->super.id_count);
	copy.version_major = htole16(info->super.version_major);
	copy.version_minor = htole16(info->super.version_minor);
	copy.root_inode_ref = htole64(info->super.root_inode_ref);
	copy.bytes_used = htole64(info->super.bytes_used);
	copy.id_table_start = htole64(info->super.id_table_start);
	copy.xattr_id_table_start = htole64(info->super.xattr_id_table_start);
	copy.inode_table_start = htole64(info->super.inode_table_start);
	copy.directory_table_start = htole64(info->super.directory_table_start);
	copy.fragment_table_start = htole64(info->super.fragment_table_start);
	copy.export_table_start = htole64(info->super.export_table_start);

	off = lseek(info->outfd, 0, SEEK_SET);
	if (off == (off_t)-1) {
		perror("seek on output file");
		return -1;
	}

	ret = write_retry(info->outfd, &copy, sizeof(copy));

	if (ret < 0) {
		perror("Error writing squashfs super block");
		return -1;
	}

	if (ret == 0) {
		fputs("Truncated write trying to write squashfs super block\n",
		      stderr);
		return -1;
	}

	return 0;
}

compressor_stream_t *sqfs_get_compressor(sqfs_super_t *s)
{
	PKG_COMPRESSION id;
	compressor_t *cmp;

	switch (s->compression_id) {
	case SQFS_COMP_GZIP:
		id = PKG_COMPRESSION_ZLIB;
		break;
	case SQFS_COMP_XZ:
		id = PKG_COMPRESSION_LZMA;
		break;
	default:
		fputs("unsupported compressor\n", stderr);
		return NULL;
	}

	cmp = compressor_by_id(id);
	if (cmp == NULL)
		return NULL;

	return cmp->compression_stream(cmp);
}
