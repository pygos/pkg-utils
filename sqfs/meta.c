/* SPDX-License-Identifier: ISC */
#include "pkg2sqfs.h"

static size_t hard_link_count(node_t *node)
{
	size_t count;
	node_t *n;

	if (S_ISDIR(node->mode)) {
		count = 2;

		for (n = node->data.dir->children; n != NULL; n = n->next)
			++count;

		return count;
	}

	return 1;
}

static int write_dir(meta_writer_t *dm, node_t *node)
{
	sqfs_dir_header_t hdr;
	sqfs_dir_entry_t ent;
	size_t i, count;
	node_t *c, *d;

	c = node->data.dir->children;
	node->data.dir->size = 0;

	node->data.dir->dref = (dm->written << 16) | dm->offset;

	while (c != NULL) {
		count = 0;

		for (d = c; d != NULL; d = d->next) {
			if ((d->inode_ref >> 16) != (c->inode_ref >> 16))
				break;
			if ((d->inode_num - c->inode_num) > 0xFFFF)
				break;
			count += 1;
		}

		if (count > SQFS_MAX_DIR_ENT)
			count = SQFS_MAX_DIR_ENT;

		hdr.count = htole32(count - 1);
		hdr.start_block = htole32(c->inode_ref >> 16);
		hdr.inode_number = htole32(c->inode_num);
		node->data.dir->size += sizeof(hdr);

		if (meta_writer_append(dm, &hdr, sizeof(hdr)))
			return -1;

		d = c;

		for (i = 0; i < count; ++i) {
			ent.offset = htole16(c->inode_ref & 0x0000FFFF);
			ent.inode_number = htole16(c->inode_num - d->inode_num);
			ent.type = htole16(c->type);
			ent.size = htole16(strlen(c->name) - 1);
			node->data.dir->size += sizeof(ent) + strlen(c->name);

			if (meta_writer_append(dm, &ent, sizeof(ent)))
				return -1;
			if (meta_writer_append(dm, c->name, strlen(c->name)))
				return -1;

			c = c->next;
		}
	}
	return 0;
}

static int write_inode(sqfs_info_t *info, meta_writer_t *im, meta_writer_t *dm,
		       node_t *node)
{
	file_info_t *fi = NULL;
	sqfs_inode_t base;
	uint32_t bs;
	uint64_t i;

	node->inode_ref = (im->written << 16) | im->offset;

	base.mode = htole16(node->mode);
	base.uid_idx = htole16(node->uid_idx);
	base.gid_idx = htole16(node->gid_idx);
	base.mod_time = htole32(info->super.modification_time);
	base.inode_number = htole32(node->inode_num);

	switch (node->mode & S_IFMT) {
	case S_IFLNK: node->type = SQFS_INODE_SLINK; break;
	case S_IFBLK: node->type = SQFS_INODE_BDEV; break;
	case S_IFCHR: node->type = SQFS_INODE_CDEV; break;
	case S_IFDIR:
		if (write_dir(dm, node))
			return -1;

		node->type = SQFS_INODE_DIR;
		i = node->data.dir->dref;

		if ((i >> 16) > 0xFFFFFFFFUL || node->data.dir->size > 0xFFFF)
			node->type = SQFS_INODE_EXT_DIR;
		break;
	case S_IFREG:
		fi = node->data.file;
		node->type = SQFS_INODE_FILE;

		if (fi->startblock > 0xFFFFFFFFUL || fi->size > 0xFFFFFFFFUL ||
		    hard_link_count(node) > 1) {
			node->type = SQFS_INODE_EXT_FILE;
		}
		break;
	default:
		assert(0);
	}

	base.type = htole16(node->type);

	if (meta_writer_append(im, &base, sizeof(base)))
		return -1;

	switch (node->type) {
	case SQFS_INODE_SLINK: {
		sqfs_inode_slink_t slink = {
			.nlink = htole32(hard_link_count(node)),
			.target_size = htole32(strlen(node->data.symlink)),
		};

		if (meta_writer_append(im, &slink, sizeof(slink)))
			return -1;
		if (meta_writer_append(im, node->data.symlink,
				       le32toh(slink.target_size))) {
			return -1;
		}
		break;
	}
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV: {
		sqfs_inode_dev_t dev = {
			.nlink = htole32(hard_link_count(node)),
			.devno = htole32(node->data.device),
		};

		return meta_writer_append(im, &dev, sizeof(dev));
	}
	case SQFS_INODE_EXT_FILE: {
		sqfs_inode_file_ext_t ext = {
			.blocks_start = htole64(fi->startblock),
			.file_size = htole64(fi->size),
			.sparse = htole64(0xFFFFFFFFFFFFFFFFUL),
			.nlink = htole32(hard_link_count(node)),
			.fragment_idx = htole32(fi->fragment),
			.fragment_offset = htole32(fi->fragment_offset),
			.xattr_idx = htole32(0xFFFFFFFF),
		};

		if (meta_writer_append(im, &ext, sizeof(ext)))
			return -1;
		goto out_file_blocks;
	}
	case SQFS_INODE_FILE: {
		sqfs_inode_file_t reg = {
			.blocks_start = htole32(fi->startblock),
			.fragment_index = htole32(fi->fragment),
			.fragment_offset = htole32(fi->fragment_offset),
			.file_size = htole32(fi->size),
		};

		if (meta_writer_append(im, &reg, sizeof(reg)))
			return -1;
		goto out_file_blocks;
	}
	case SQFS_INODE_DIR: {
		sqfs_inode_dir_t dir = {
			.start_block = htole32(node->data.dir->dref >> 16),
			.nlink = htole32(hard_link_count(node)),
			.size = htole16(node->data.dir->size),
			.offset = htole16(node->data.dir->dref & 0xFFFF),
			.parent_inode = node->parent ?
				htole32(node->parent->inode_num) : htole32(1),
		};

		return meta_writer_append(im, &dir, sizeof(dir));
	}
	case SQFS_INODE_EXT_DIR: {
		sqfs_inode_dir_ext_t ext = {
			.nlink = htole32(hard_link_count(node)),
			.size = htole32(node->data.dir->size),
			.start_block = htole32(node->data.dir->dref >> 16),
			.parent_inode = node->parent ?
				htole32(node->parent->inode_num) : htole32(1),
			.inodex_count = htole32(0),
			.offset = htole16(node->data.dir->dref & 0xFFFF),
			.xattr_idx = htole32(0xFFFFFFFF),
		};

		return meta_writer_append(im, &ext, sizeof(ext));
	}
	default:
		assert(0);
	}
	return 0;
out_file_blocks:
	for (i = 0; i < fi->size / info->super.block_size; ++i) {
		bs = htole32(fi->blocksizes[i]);

		if (meta_writer_append(im, &bs, sizeof(bs)))
			return -1;
	}
	return 0;
}

int sqfs_write_inodes(sqfs_info_t *info, compressor_stream_t *strm)
{
	meta_writer_t *im, *dm;
	size_t i, diff;
	ssize_t ret;
	FILE *tmp;
	int tmpfd;

	tmp = tmpfile();
	if (tmp == NULL) {
		perror("tmpfile");
		return -1;
	}

	tmpfd = fileno(tmp);

	im = meta_writer_create(info->outfd, strm);
	if (im == NULL)
		goto fail_tmp;

	dm = meta_writer_create(tmpfd, strm);
	if (dm == NULL)
		goto fail_im;

	for (i = 2; i < info->fs.num_inodes; ++i) {
		if (write_inode(info, im, dm, info->fs.node_tbl[i]))
			goto fail;
	}

	if (meta_writer_flush(im))
		goto fail;

	if (meta_writer_flush(dm))
		goto fail;

	info->super.root_inode_ref = info->fs.root->inode_ref;

	info->super.inode_table_start = info->super.bytes_used;
	info->super.bytes_used += im->written;

	info->super.directory_table_start = info->super.bytes_used;
	info->super.bytes_used += dm->written;

	if (lseek(tmpfd, 0, SEEK_SET) == (off_t)-1) {
		perror("rewind on directory temp file");
		goto fail;
	}

	for (;;) {
		ret = read_retry(tmpfd, dm->data, sizeof(dm->data));

		if (ret < 0) {
			perror("read from temp file");
			goto fail;
		}
		if (ret == 0)
			break;

		diff = ret;
		ret = write_retry(info->outfd, dm->data, diff);

		if (ret < 0) {
			perror("write to image file");
			goto fail;
		}
		if ((size_t)ret < diff) {
			fputs("copying meta data to image file: "
			      "truncated write\n", stderr);
			goto fail;
		}
	}

	meta_writer_destroy(dm);
	meta_writer_destroy(im);
	fclose(tmp);
	return 0;
fail:
	meta_writer_destroy(dm);
fail_im:
	meta_writer_destroy(im);
fail_tmp:
	fclose(tmp);
	return -1;
}
