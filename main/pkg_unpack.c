/* SPDX-License-Identifier: ISC */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "pkgio.h"
#include "util/util.h"

static int create_hierarchy(int dirfd, image_entry_t *list, int flags)
{
	image_entry_t *ent;

	for (ent = list; ent != NULL; ent = ent->next) {
		if (S_ISDIR(ent->mode)) {
			if (mkdirat(dirfd, ent->name, 0755)) {
				if (errno == EEXIST)
					continue;
				fprintf(stderr, "mkdir %s: %s\n", ent->name,
					strerror(errno));
				return -1;
			}
		}
	}

	for (ent = list; ent != NULL; ent = ent->next) {
		if (S_ISLNK(ent->mode)) {
			if (flags & UNPACK_NO_SYMLINKS)
				continue;

			if (symlinkat(ent->data.symlink.target,
				      dirfd, ent->name)) {
				fprintf(stderr, "symlink %s to %s: %s\n",
					ent->name, ent->data.symlink.target,
					strerror(errno));
				return -1;
			}
		}
	}

	for (ent = list; ent != NULL; ent = ent->next) {
		if (S_ISBLK(ent->mode) || S_ISCHR(ent->mode)) {
			if (flags & UNPACK_NO_DEVICES)
				continue;

			if (mknodat(dirfd, ent->name, ent->mode,
				    ent->data.device.devno)) {
				fprintf(stderr, "mknod %s: %s\n",
					ent->name, strerror(errno));
				return -1;
			}
		}
	}

	return 0;
}

static image_entry_t *get_file_entry(image_entry_t *list, uint32_t id)
{
	while (list != NULL) {
		if (S_ISREG(list->mode) && list->data.file.id == id)
			return list;

		list = list->next;
	}

	return NULL;
}

static int unpack_files(int dirfd, image_entry_t *list, pkg_reader_t *rd)
{
	uint8_t buffer[2048];
	image_entry_t *meta;
	file_data_t frec;
	ssize_t ret;
	size_t diff;
	uint64_t i;
	int fd;

	for (;;) {
		ret = pkg_reader_read_payload(rd, &frec, sizeof(frec));
		if (ret == 0)
			break;
		if (ret < 0)
			return -1;
		if ((size_t)ret < sizeof(frec))
			goto fail_trunc;

		frec.id = le32toh(frec.id);

		meta = get_file_entry(list, frec.id);
		if (meta == NULL) {
			fprintf(stderr, "%s: missing meta information for "
				"file %u\n", pkg_reader_get_filename(rd),
				(unsigned int)frec.id);
			return -1;
		}

		fd = openat(dirfd, meta->name, O_WRONLY | O_CREAT | O_EXCL,
			    0644);
		if (fd < 0) {
			perror(meta->name);
			return -1;
		}

		for (i = 0; i < meta->data.file.size; i += ret) {
			if ((meta->data.file.size - i) <
			    (uint64_t)sizeof(buffer)) {
				diff = meta->data.file.size - i;
			} else {
				diff = sizeof(buffer);
			}

			ret = pkg_reader_read_payload(rd, buffer, diff);
			if (ret < 0)
				goto fail_fd;
			if ((size_t)ret < diff)
				goto fail_trunc;

			ret = write_retry(fd, buffer, diff);
			if (ret < 0) {
				perror(meta->name);
				goto fail_fd;
			}

			if ((size_t)ret < diff) {
				fprintf(stderr, "%s: truncated write\n",
					pkg_reader_get_filename(rd));
				goto fail_fd;
			}
		}

		close(fd);
	}

	return 0;
fail_fd:
	close(fd);
	return -1;
fail_trunc:
	fprintf(stderr, "%s: truncated file data record\n",
		pkg_reader_get_filename(rd));
	return -1;
}

static int change_permissions(int dirfd, image_entry_t *list, int flags)
{
	bool do_chmod, do_chown;

	for (; list != NULL; list = list->next) {
		do_chmod = (flags & UNPACK_NO_CHMOD) == 0;
		do_chown = (flags & UNPACK_NO_CHOWN) == 0;

		switch (list->mode & S_IFMT) {
		case S_IFLNK:
			if (flags & UNPACK_NO_SYMLINKS)
				continue;
			do_chmod = false;
			break;
		case S_IFBLK:
		case S_IFCHR:
			if (flags & UNPACK_NO_DEVICES)
				continue;
			do_chmod = false;
			break;
		default:
			break;
		}

		if (do_chmod) {
			if (fchmodat(dirfd, list->name,
				     list->mode & 07777, 0)) {
				fprintf(stderr, "%s: chmod: %s\n", list->name,
					strerror(errno));
				return -1;
			}
		}

		if (do_chown) {
			if (fchownat(dirfd, list->name, list->uid, list->gid,
				     AT_SYMLINK_NOFOLLOW)) {
				fprintf(stderr, "%s: chown: %s\n", list->name,
					strerror(errno));
				return -1;
			}
		}
	}

	return 0;
}

int pkg_unpack(int rootfd, int flags, pkg_reader_t *rd)
{
	image_entry_t *list = NULL;
	record_t *hdr;
	int ret;

	if (image_entry_list_from_package(rd, &list))
		return -1;

	if (pkg_reader_rewind(rd))
		goto fail;

	if (list == NULL)
		return 0;

	if (create_hierarchy(rootfd, list, flags))
		goto fail;

	for (;;) {
		ret = pkg_reader_get_next_record(rd);
		if (ret == 0)
			break;
		if (ret < 0)
			goto fail;

		hdr = pkg_reader_current_record_header(rd);

		if (hdr->magic == PKG_MAGIC_DATA) {
			if (unpack_files(rootfd, list, rd))
				goto fail;
		}
	}

	if (change_permissions(rootfd, list, flags))
		goto fail;

	image_entry_free_list(list);
	return 0;
fail:
	image_entry_free_list(list);
	return -1;
}
