/* SPDX-License-Identifier: ISC */
#include "pack.h"

static int write_file(pkg_writer_t *wr, image_entry_t *ent)
{
	uint64_t offset = 0;
	uint8_t buffer[1024];
	file_data_t fdata;
	ssize_t ret;
	int fd;

	memset(&fdata, 0, sizeof(fdata));

	fdata.id = htole32(ent->data.file.id);

	if (pkg_writer_write_payload(wr, &fdata, sizeof(fdata)))
		return -1;

	fd = open(ent->data.file.location, O_RDONLY);

	if (fd < 0) {
		perror(ent->data.file.location);
		return -1;
	}

	while (offset < ent->data.file.size) {
		ret = read_retry(fd, buffer, sizeof(buffer));

		if (ret < 0) {
			perror(ent->data.file.location);
			goto fail_fd;
		}

		if (ret == 0)
			break;

		if (pkg_writer_write_payload(wr, buffer, ret))
			goto fail_fd;

		offset += (uint64_t)ret;
	}

	close(fd);
	return 0;
fail_fd:
	close(fd);
	return -1;
}

int write_files(pkg_writer_t *wr, image_entry_t *list, compressor_t *cmp)
{
	if (pkg_writer_start_record(wr, PKG_MAGIC_DATA, cmp))
		return -1;

	while (list != NULL) {
		if (S_ISREG(list->mode)) {
			if (write_file(wr, list))
				return -1;
		}

		list = list->next;
	}

	return pkg_writer_end_record(wr);
}
