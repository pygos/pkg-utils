#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>

#include "pkgio.h"
#include "util.h"

static int read_extra(pkg_reader_t *pkg, image_entry_t *ent)
{
	switch (ent->mode & S_IFMT) {
	case S_IFLNK: {
		toc_symlink_extra_t extra;
		char *path;
		int ret;

		ret = pkg_reader_read_payload(pkg, &extra, sizeof(extra));
		if (ret < 0)
			return -1;
		if ((size_t)ret < sizeof(extra))
			goto fail_trunc;

		extra.target_length = le16toh(extra.target_length);

		path = malloc(extra.target_length + 1);
		if (path == NULL)
			goto fail_oom;

		ret = pkg_reader_read_payload(pkg, path, extra.target_length);
		if (ret < 0)
			return -1;
		if ((size_t)ret < extra.target_length)
			goto fail_trunc;

		path[extra.target_length] = '\0';
		ent->data.symlink.target = path;
		break;
	}
	case S_IFREG: {
		toc_file_extra_t extra;
		int ret;

		ret = pkg_reader_read_payload(pkg, &extra, sizeof(extra));
		if (ret < 0)
			return -1;
		if ((size_t)ret < sizeof(extra))
			goto fail_trunc;

		ent->data.file.size = le64toh(extra.size);
		ent->data.file.id = le32toh(extra.id);
		break;
	}
	case S_IFDIR:
		break;
	default:
		goto fail_unknown;
	}
	return 0;
fail_unknown:
	fprintf(stderr, "%s: unsupported file type in table of contents\n",
		pkg_reader_get_filename(pkg));
	return -1;
fail_oom:
	fputs("out of memory\n", stderr);
	return -1;
fail_trunc:
	fprintf(stderr, "%s: truncated extra data in table of contents\n",
		pkg_reader_get_filename(pkg));
	return -1;
}

image_entry_t *image_entry_list_from_package(pkg_reader_t *pkg)
{
	image_entry_t *list = NULL, *end = NULL, *imgent;
	toc_entry_t ent;
	record_t *hdr;
	ssize_t ret;
	char *path;

	if (pkg_reader_rewind(pkg))
		return NULL;

	do {
		if (pkg_reader_get_next_record(pkg) <= 0)
			return NULL;

		hdr = pkg_reader_current_record_header(pkg);
	} while (hdr->magic != PKG_MAGIC_TOC);

	for (;;) {
		ret = pkg_reader_read_payload(pkg, &ent, sizeof(ent));
		if (ret == 0)
			break;
		if (ret < 0)
			goto fail;
		if ((size_t)ret < sizeof(ent))
			goto fail_trunc;

		ent.mode = le32toh(ent.mode);
		ent.uid = le32toh(ent.uid);
		ent.gid = le32toh(ent.gid);
		ent.path_length = le16toh(ent.path_length);

		path = malloc(ent.path_length + 1);
		if (path == NULL)
			goto fail_oom;

		ret = pkg_reader_read_payload(pkg, path, ent.path_length);
		if (ret < 0)
			goto fail;
		if ((size_t)ret < ent.path_length)
			goto fail_trunc;
		path[ent.path_length] = '\0';

		if (canonicalize_name(path)) {
			fprintf(stderr,
				"%s: invalid file path '%s' in package\n",
				pkg_reader_get_filename(pkg), path);
			free(path);
			goto fail;
		}

		imgent = calloc(1, sizeof(*imgent));
		if (imgent == NULL) {
			free(path);
			goto fail_oom;
		}

		imgent->name = path;
		imgent->mode = ent.mode;
		imgent->uid = ent.uid;
		imgent->gid = ent.gid;

		if (read_extra(pkg, imgent)) {
			image_entry_free(imgent);
			goto fail;
		}

		if (list) {
			end->next = imgent;
			end = imgent;
		} else {
			list = end = imgent;
		}
	}

	for (;;) {
		ret = pkg_reader_get_next_record(pkg);
		if (ret == 0)
			break;
		if (ret < 0)
			goto fail;

		hdr = pkg_reader_current_record_header(pkg);
		if (hdr->magic == PKG_MAGIC_TOC)
			goto fail_multi;
	}

	return list;
fail_oom:
	fputs("out of memory\n", stderr);
	goto fail;
fail_trunc:
	fprintf(stderr, "%s: truncated entry in table of contents\n",
		pkg_reader_get_filename(pkg));
	goto fail;
fail_multi:
	fprintf(stderr, "%s: multiple table of contents entries found\n",
		pkg_reader_get_filename(pkg));
	goto fail;
fail:
	image_entry_free_list(list);
	return NULL;
}
