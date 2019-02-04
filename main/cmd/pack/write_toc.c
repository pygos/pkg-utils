#include "pack.h"

static int write_entry(pkg_writer_t *wr, image_entry_t *it)
{
	toc_entry_t ent;
	size_t len;

	len = strlen(it->name);

	memset(&ent, 0, sizeof(ent));
	ent.mode        = htole32(it->mode);
	ent.uid         = htole32(it->uid);
	ent.gid         = htole32(it->gid);
	ent.path_length = htole16(len);

	if (pkg_writer_write_payload(wr, &ent, sizeof(ent)))
		return -1;

	if (pkg_writer_write_payload(wr, it->name, len))
		return -1;

	switch (it->mode & S_IFMT) {
	case S_IFREG: {
		toc_file_extra_t file;

		memset(&file, 0, sizeof(file));
		file.size = htole64(it->data.file.size);
		file.id = htole32(it->data.file.id);

		if (pkg_writer_write_payload(wr, &file, sizeof(file)))
			return -1;
		break;
	}
	case S_IFLNK: {
		toc_symlink_extra_t sym;

		len = strlen(it->data.symlink.target);

		memset(&sym, 0, sizeof(sym));
		sym.target_length = htole16(len);

		if (pkg_writer_write_payload(wr, &sym, sizeof(sym)))
			return -1;

		if (pkg_writer_write_payload(wr, it->data.symlink.target, len))
			return -1;
		break;
	}
	case S_IFDIR:
		break;
	case S_IFBLK:
	case S_IFCHR: {
		toc_device_extra_t dev;

		dev.devno = htole64(it->data.device.devno);

		if (pkg_writer_write_payload(wr, &dev, sizeof(dev)))
			return -1;
		break;
	}
	default:
		assert(0);
	}

	return 0;
}

int write_toc(pkg_writer_t *wr, image_entry_t *list, compressor_t *cmp)
{
	if (pkg_writer_start_record(wr, PKG_MAGIC_TOC, cmp))
		return -1;

	while (list != NULL) {
		if (write_entry(wr, list))
			return -1;

		list = list->next;
	}

	return pkg_writer_end_record(wr);
}
