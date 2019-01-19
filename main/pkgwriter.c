#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "pkgwriter.h"
#include "util.h"

struct pkg_writer_t {
	const char *path;
	int fd;

	off_t start;
	record_t current;
	compressor_stream_t *stream;
};

static int write_header(pkg_writer_t *wr)
{
	ssize_t ret;

	wr->current.magic = htole32(wr->current.magic);
	wr->current.compressed_size = htole64(wr->current.compressed_size);
	wr->current.raw_size = htole64(wr->current.raw_size);

	ret = write_retry(wr->fd, &wr->current, sizeof(wr->current));
	if (ret < 0)
		goto fail_fop;

	if ((size_t)ret < sizeof(wr->current)) {
		fprintf(stderr,
			"record header was truncated while writing to %s\n",
			wr->path);
		return -1;
	}

	return 0;
fail_fop:
	perror(wr->path);
	return -1;
}

static int flush_to_file(pkg_writer_t *wr)
{
	uint8_t buffer[2048];
	ssize_t count, ret;

	for (;;) {
		count = wr->stream->read(wr->stream, buffer, sizeof(buffer));
		if (count == 0)
			break;
		if (count < 0) {
			fprintf(stderr, "%s: error compressing data\n",
				wr->path);
			return -1;
		}

		ret = write_retry(wr->fd, buffer, count);

		if (ret < 0) {
			fprintf(stderr, "%s: writing to package file: %s\n",
				wr->path, strerror(errno));
			return -1;
		}

		if (ret < count) {
			fprintf(stderr,
				"%s: data written to file was truncated\n",
				wr->path);
			return -1;
		}

		wr->current.compressed_size += count;
	}

	return 0;
}

pkg_writer_t *pkg_writer_open(const char *path)
{
	pkg_writer_t *wr = calloc(1, sizeof(*wr));

	if (wr == NULL) {
		fputs("out of memory\n", stderr);
		return NULL;
	}

	wr->fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (wr->fd == -1) {
		perror(path);
		free(wr);
		return NULL;
	}

	wr->path = path;

	wr->current.magic = PKG_MAGIC_HEADER;
	wr->current.compression = PKG_COMPRESSION_NONE;
	if (write_header(wr)) {
		close(wr->fd);
		free(wr);
		return NULL;
	}

	return wr;
}

void pkg_writer_close(pkg_writer_t *wr)
{
	close(wr->fd);
	free(wr);
}

int pkg_writer_start_record(pkg_writer_t *wr, uint32_t magic,
			    compressor_t *cmp)
{
	wr->start = lseek(wr->fd, 0, SEEK_CUR);
	if (wr->start == -1) {
		perror(wr->path);
		return -1;
	}

	memset(&wr->current, 0, sizeof(wr->current));
	if (write_header(wr))
		return -1;

	wr->stream = cmp->compression_stream(cmp);
	if (wr->stream == NULL)
		return -1;

	wr->current.magic = magic;
	wr->current.compression = cmp->id;
	return 0;
}

int pkg_writer_write_payload(pkg_writer_t *wr, void *data, size_t size)
{
	ssize_t ret;

	while (size > 0) {
		ret = wr->stream->write(wr->stream, data, size);
		if (ret < 0)
			return -1;

		if ((size_t)ret < size) {
			if (flush_to_file(wr))
				return -1;
		}

		data = (char *)data + ret;
		size -= ret;
		wr->current.raw_size += ret;
	}

	return 0;
}

int pkg_writer_end_record(pkg_writer_t *wr)
{
	wr->stream->flush(wr->stream);
	if (flush_to_file(wr))
		return -1;

	if (lseek(wr->fd, wr->start, SEEK_SET) == -1)
		goto fail_seek;

	if (write_header(wr))
		return -1;

	if (lseek(wr->fd, 0, SEEK_END) == -1)
		goto fail_seek;

	wr->stream->destroy(wr->stream);
	wr->stream = NULL;
	return 0;
fail_seek:
	perror(wr->path);
	return -1;
}
