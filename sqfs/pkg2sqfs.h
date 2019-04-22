/* SPDX-License-Identifier: ISC */
#ifndef PKG2SQFS_H
#define PKG2SQFS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include "util/util.h"

#include "filelist/image_entry.h"
#include "comp/compressor.h"
#include "pkg/pkgreader.h"
#include "sqfs/squashfs.h"
#include "pkg/pkgio.h"

#include "config.h"

typedef struct file_info_t {
	struct file_info_t *frag_next;
	uint64_t size;
	uint64_t startblock;
	uint32_t id;
	uint32_t fragment;
	uint32_t fragment_offset;
	uint32_t blocksizes[];
} file_info_t;

typedef struct {
	struct node_t *children;
	uint64_t dref;
	size_t size;
} dir_info_t;

typedef struct node_t {
	struct node_t *parent;
	struct node_t *next;
	const char *name;

	union {
		dir_info_t *dir;
		const char *symlink;
		dev_t device;
		file_info_t *file;
	} data;

	uint64_t inode_ref;
	uint32_t inode_num;
	uint16_t type;
	uint16_t mode;
	uint16_t uid_idx;
	uint16_t gid_idx;

	uint8_t extra[];
} node_t;

typedef struct {
	uint32_t default_uid;
	uint32_t default_gid;
	uint32_t default_mode;

	node_t **node_tbl;
	size_t num_inodes;

	uint32_t *id_tbl;
	size_t num_ids;
	size_t max_ids;

	node_t *root;
} vfs_t;

typedef struct {
	int outfd;
	sqfs_super_t super;
	vfs_t fs;
	pkg_reader_t *rd;
	void *block;
	void *scratch;
	void *fragment;

	sqfs_fragment_t *fragments;
	size_t num_fragments;
	size_t max_fragments;

	int file_block_count;
	file_info_t *frag_list;
	size_t frag_offset;

	size_t dev_blk_size;
} sqfs_info_t;

int pkg_data_to_sqfs(sqfs_info_t *info, compressor_stream_t *strm);

int create_vfs_tree(sqfs_info_t *info);

void destroy_vfs_tree(vfs_t *fs);

node_t *vfs_node_from_file_id(vfs_t *fs, uint32_t id);

int sqfs_write_inodes(sqfs_info_t *info, compressor_stream_t *strm);

#endif /* PKG2SQFS_H */
