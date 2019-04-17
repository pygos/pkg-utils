/* SPDX-License-Identifier: ISC */
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "pkg2sqfs.h"

static int id_to_index(vfs_t *fs, uint32_t id, uint16_t *out)
{
	size_t i, new_sz;
	void *new;

	for (i = 0; i < fs->num_ids; ++i) {
		if (fs->id_tbl[i] == id) {
			*out = i;
			return 0;
		}
	}

	if (fs->num_ids > 0xFFFF) {
		fputs("too many unique UIDs/GIDs (more than 64k)!\n", stderr);
		return -1;
	}

	if (fs->num_ids == fs->max_ids) {
		new_sz = fs->max_ids + 10;
		new = realloc(fs->id_tbl, new_sz * sizeof(fs->id_tbl[0]));

		if (new == NULL) {
			perror("expanding ID table");
			return -1;
		}

		fs->max_ids = new_sz;
		fs->id_tbl = new;
	}

	*out = fs->num_ids;
	fs->id_tbl[fs->num_ids++] = id;
	return 0;
}

static node_t *node_create(vfs_t *fs, node_t *parent, size_t extra,
			   const char *name, size_t name_len,
			   uint16_t mode, uint32_t uid, uint32_t gid)
{
	node_t *n = calloc(1, sizeof(*n) + extra + name_len + 1);

	if (n == NULL) {
		perror("allocating vfs node");
		return NULL;
	}

	if (parent != NULL) {
		n->parent = parent;
		n->next = parent->data.dir->children;
		parent->data.dir->children = n;
	}

	if (id_to_index(fs, uid, &n->uid_idx))
		return NULL;

	if (id_to_index(fs, gid, &n->gid_idx))
		return NULL;

	n->mode = mode;
	n->name = (char *)n->extra;
	memcpy(n->extra, name, name_len);
	return n;
}

static node_t *node_from_ent(vfs_t *fs, sqfs_super_t *s, image_entry_t *ent)
{
	node_t *n, *parent = fs->root;
	const char *path = ent->name;
	size_t len, extra;
	char *end;
next:
	end = strchrnul(path, '/');
	len = end - path;

	for (n = parent->data.dir->children; n != NULL; n = n->next) {
		if (!strncmp(n->name, path, len) && strlen(n->name) == len) {
			if (path[len] != '/')
				goto fail_exists;
			if (!S_ISDIR(n->mode))
				goto fail_not_dir;
			parent = n;
			path += len + 1;
			goto next;
		}
	}

	if (path[len] == '/') {
		if (S_ISDIR(ent->mode)) {
			n = node_create(fs, parent, sizeof(dir_info_t),
					path, len, S_IFDIR | fs->default_mode,
					fs->default_uid, fs->default_gid);
			if (n == NULL)
				return NULL;

			n->data.dir =
				(dir_info_t *)(n->name + strlen(n->name) + 1);

			parent = n;
			path += len + 1;
			goto next;
		}
		goto fail_no_ent;
	}

	switch (ent->mode & S_IFMT) {
	case S_IFREG:
		extra = sizeof(file_info_t);

		extra += (ent->data.file.size / s->block_size) *
			sizeof(uint32_t);
		break;
	case S_IFLNK:
		extra = strlen(ent->data.symlink.target) + 1;
		break;
	case S_IFDIR:
		extra = sizeof(dir_info_t);
		break;
	default:
		extra = 0;
		break;
	}

	n = node_create(fs, parent, extra, path, len,
			ent->mode, ent->uid, ent->gid);
	if (n == NULL)
		return NULL;

	switch (ent->mode & S_IFMT) {
	case S_IFREG:
		n->data.file = (file_info_t *)(n->name + strlen(n->name) + 1);
		n->data.file->size = ent->data.file.size;
		n->data.file->id = ent->data.file.id;
		n->data.file->startblock = 0xFFFFFFFFFFFFFFFFUL;
		n->data.file->fragment = 0xFFFFFFFFL;
		break;
	case S_IFLNK:
		n->data.symlink = n->name + strlen(n->name) + 1;
		strcpy((char *)n->data.symlink, ent->data.symlink.target);
		break;
	case S_IFBLK:
	case S_IFCHR:
		n->data.device = ent->data.device.devno;
		break;
	case S_IFDIR:
		n->data.dir = (dir_info_t *)(n->name + strlen(n->name) + 1);
		break;
	default:
		break;
	}

	return n;
fail_no_ent:
	fprintf(stderr, "cannot create /%s: '%.*s' does not exist\n",
		ent->name, (int)len, path);
	return NULL;
fail_exists:
	fprintf(stderr, "cannot create /%s: already exists\n", ent->name);
	return NULL;
fail_not_dir:
	fprintf(stderr, "cannot create /%s: %.*s is not a directory\n",
		ent->name, (int)len, path);
	return NULL;
}

static void node_recursive_delete(node_t *n)
{
	node_t *child;

	if (n != NULL && S_ISDIR(n->mode)) {
		while (n->data.dir->children != NULL) {
			child = n->data.dir->children;
			n->data.dir->children = child->next;

			node_recursive_delete(child);
		}
	}

	free(n);
}

typedef int(*node_cb)(vfs_t *fs, node_t *n, void *user);

static int foreach_node(vfs_t *fs, node_t *root, void *user, node_cb cb)
{
	bool has_subdirs = false;
	node_t *it;

	for (it = root->data.dir->children; it != NULL; it = it->next) {
		if (S_ISDIR(it->mode)) {
			has_subdirs = true;
			break;
		}
	}

	if (has_subdirs) {
		for (it = root->data.dir->children; it != NULL; it = it->next) {
			if (!S_ISDIR(it->mode))
				continue;

			if (foreach_node(fs, it, user, cb) != 0)
				return -1;
		}
	}

	for (it = root->data.dir->children; it != NULL; it = it->next) {
		if (cb(fs, it, user))
			return -1;
	}

	if (root->parent == NULL) {
		if (cb(fs, root, user))
			return -1;
	}

	return 0;
}

static int alloc_inum(vfs_t *fs, node_t *n, void *user)
{
	uint32_t *counter = user;
	(void)fs;

	if (*counter == 0xFFFFFFFF) {
		fputs("too many inodes (more than 2^32 - 2)!\n", stderr);
		return -1;
	}

	n->inode_num = (*counter)++;
	return 0;
}

static int link_node_to_tbl(vfs_t *fs, node_t *n, void *user)
{
	(void)user;
	fs->node_tbl[n->inode_num] = n;
	return 0;
}

static node_t *sort_list(node_t *head)
{
	node_t *it, *prev, *lhs, *rhs;
	size_t i, count = 0;

	for (it = head; it != NULL; it = it->next)
		++count;

	if (count < 2)
		return head;

	prev = NULL;
	it = head;

	for (i = 0; i < count / 2; ++i) {
		prev = it;
		it = it->next;
	}

	prev->next = NULL;

	lhs = sort_list(head);
	rhs = sort_list(it);

	head = NULL;
	prev = NULL;

	while (lhs != NULL && rhs != NULL) {
		if (strcmp(lhs->name, rhs->name) <= 0) {
			it = lhs;
			lhs = lhs->next;
		} else {
			it = rhs;
			rhs = rhs->next;
		}

		it->next = NULL;
		if (prev != NULL) {
			prev->next = it;
			prev = it;
		} else {
			prev = head = it;
		}
	}

	it = (lhs != NULL ? lhs : rhs);

	if (prev != NULL) {
		prev->next = it;
	} else {
		head = it;
	}

	return head;
}

static void sort_directory(node_t *n)
{
	n->data.dir->children = sort_list(n->data.dir->children);

	for (n = n->data.dir->children; n != NULL; n = n->next) {
		if (S_ISDIR(n->mode))
			sort_directory(n);
	}
}

int create_vfs_tree(sqfs_info_t *info)
{
	image_entry_t *ent, *list;
	uint32_t counter = 2;
	vfs_t *fs = &info->fs;
	node_t *n;
	size_t i;

	if (image_entry_list_from_package(info->rd, &list))
		return -1;

	fs->root = node_create(fs, NULL, sizeof(dir_info_t),
			       "", 0, S_IFDIR | 0755, 0, 0);
	if (fs->root == NULL)
		goto fail;

	fs->root->data.dir = (dir_info_t *)(fs->root->name + strlen(fs->root->name) + 1);

	for (ent = list; ent != NULL; ent = ent->next) {
		n = node_from_ent(fs, &info->super, ent);
		if (n == NULL)
			goto fail;
	}

	sort_directory(fs->root);

	if (foreach_node(fs, fs->root, &counter, alloc_inum))
		goto fail;

	fs->num_inodes = counter;
	fs->node_tbl = calloc(fs->num_inodes, sizeof(fs->node_tbl[0]));
	if (fs->node_tbl == NULL)
		goto fail_oom;

	if (foreach_node(fs, fs->root, NULL, link_node_to_tbl))
		goto fail;

	info->super.inode_count = fs->num_inodes - 2;
	info->super.id_count = fs->num_ids;

	image_entry_free_list(list);

	for (i = 0; i < fs->num_ids; ++i)
		fs->id_tbl[i] = htole32(fs->id_tbl[i]);

	return 0;
fail_oom:
	fputs("out of memory\n", stderr);
fail:
	image_entry_free_list(list);
	free(fs->node_tbl);
	free(fs->id_tbl);
	node_recursive_delete(fs->root);
	return -1;
}

void destroy_vfs_tree(vfs_t *fs)
{
	free(fs->node_tbl);
	free(fs->id_tbl);
	node_recursive_delete(fs->root);
}

node_t *vfs_node_from_file_id(vfs_t *fs, uint32_t id)
{
	size_t i;

	for (i = 0; i < fs->num_inodes; ++i) {
		if (fs->node_tbl[i] == NULL)
			continue;
		if (!S_ISREG(fs->node_tbl[i]->mode))
			continue;
		if (fs->node_tbl[i]->data.file->id == id)
			return fs->node_tbl[i];
	}

	return NULL;
}
