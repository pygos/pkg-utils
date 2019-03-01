#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include "util/util.h"
#include "pkgreader.h"
#include "compressor.h"

struct pkg_reader_t {
	int fd;
	bool have_eof;
	bool have_error;
	uint64_t offset_compressed;
	uint64_t offset_raw;
	compressor_stream_t *stream;
	const char *path;

	record_t current;
};

static int read_header(pkg_reader_t *rd)
{
	ssize_t diff = read_retry(rd->fd, &rd->current, sizeof(rd->current));

	if (diff == 0) {
		rd->have_eof = true;
		return 0;
	}

	if (diff < 0)
		goto fail_io;

	if ((size_t)diff < sizeof(rd->current))
		goto fail_trunc;

	rd->offset_raw = 0;
	rd->offset_compressed = 0;

	rd->current.magic = le32toh(rd->current.magic);
	rd->current.compressed_size = le64toh(rd->current.compressed_size);
	rd->current.raw_size = le64toh(rd->current.raw_size);
	return 1;
fail_trunc:
	rd->have_error = true;
	fprintf(stderr, "%s: package file seems to be truncated\n", rd->path);
	return -1;
fail_io:
	rd->have_error = true;
	fprintf(stderr, "%s: reading from package file: %s\n",
		rd->path, strerror(errno));
	return -1;
}

static int prefetch_compressed(pkg_reader_t *rd)
{
	uint8_t buffer[1024];
	compressor_t *cmp;
	ssize_t ret;
	size_t diff;

	if (rd->stream == NULL) {
		cmp = compressor_by_id(rd->current.compression);

		if (cmp == NULL)
			goto fail_comp;

		rd->stream = cmp->uncompression_stream(cmp);
		if (rd->stream == NULL) {
			rd->have_error = true;
			return -1;
		}
	}

	while (rd->offset_compressed < rd->current.compressed_size) {
		diff = rd->current.compressed_size - rd->offset_compressed;

		if (diff > sizeof(buffer))
			diff = sizeof(buffer);

		ret = read_retry(rd->fd, buffer, diff);
		if (ret < 0)
			goto fail_io;
		if ((size_t)ret < diff)
			goto fail_trunc;

		ret = rd->stream->write(rd->stream, buffer, diff);
		if (ret < 0)
			return -1;

		rd->offset_compressed += ret;

		if ((size_t)ret < diff) {
			if (lseek(rd->fd, -((long)(diff - ret)),
				  SEEK_CUR) == -1)
				goto fail_io;
			break;
		}
	}

	return 0;
fail_trunc:
	rd->have_error = true;
	fprintf(stderr, "%s: truncated record in package file\n", rd->path);
	return -1;
fail_io:
	rd->have_error = true;
	fprintf(stderr, "%s: reading from package file: %s\n",
		rd->path, strerror(errno));
	return -1;
fail_comp:
	fprintf(stderr, "%s: package uses unsupported compression\n",
		rd->path);
	rd->have_error = true;
	return -1;
}

static pkg_reader_t *pkg_reader_openat(int dirfd, const char *path)
{
	pkg_reader_t *rd = calloc(1, sizeof(*rd));
	int ret;

	if (rd == NULL) {
		fputs("out of memory\n", stderr);
		return NULL;
	}

	rd->path = path;
	rd->fd = openat(dirfd, path, O_RDONLY);
	if (rd->fd < 0) {
		perror(path);
		free(rd);
		return NULL;
	}

	ret = read_header(rd);
	if (ret < 0)
		goto fail;
	if (ret == 0 || rd->current.magic != PKG_MAGIC_HEADER)
		goto fail_header;

	return rd;
fail_header:
	fprintf(stderr, "%s: missing package header\n", path);
fail:
	pkg_reader_close(rd);
	return NULL;
}

pkg_reader_t *pkg_reader_open(const char *path)
{
	return pkg_reader_openat(AT_FDCWD, path);
}

pkg_reader_t *pkg_reader_open_repo(int dirfd, const char *name)
{
	char *fname;
	size_t i;

	for (i = 0; name[i] != '\0'; ++i) {
		if (!isalnum(name[i]) && name[i] != '_' && name[i] != '-' &&
		    name[i] != '+') {
			fprintf(stderr,
				"illegal characters in package name '%s'\n",
				name);
			return NULL;
		}
	}

	fname = alloca(strlen(name) + 5);
	sprintf(fname, "%s.pkg", name);

	return pkg_reader_openat(dirfd, fname);
}

void pkg_reader_close(pkg_reader_t *rd)
{
	if (rd->stream != NULL)
		rd->stream->destroy(rd->stream);

	close(rd->fd);
	free(rd);
}

int pkg_reader_get_next_record(pkg_reader_t *rd)
{
	uint64_t skip;
	int ret;

	if (rd->have_eof)
		return 0;

	if (rd->have_error)
		return -1;

	skip = rd->current.compressed_size - rd->offset_compressed;

	if (lseek(rd->fd, skip, SEEK_CUR) == -1)
		goto fail_io;

	if (rd->stream != NULL) {
		rd->stream->destroy(rd->stream);
		rd->stream = NULL;
	}

	ret = read_header(rd);
	if (ret <= 0)
		return ret;

	if (rd->current.magic == PKG_MAGIC_HEADER)
		goto fail_second_hdr;

	return 1;
fail_io:
	rd->have_error = true;
	fprintf(stderr, "%s: reading from packet file: %s\n",
		rd->path, strerror(errno));
	return -1;
fail_second_hdr:
	rd->have_error = true;
	fprintf(stderr, "%s: found extra header record inside package\n",
		rd->path);
	return -1;
}

record_t *pkg_reader_current_record_header(pkg_reader_t *rd)
{
	return (rd->have_error || rd->have_eof) ? NULL : &rd->current;
}

ssize_t pkg_reader_read_payload(pkg_reader_t *rd, void *out, size_t size)
{
	ssize_t ret;

	if (rd->have_error)
		return -1;

	if (rd->have_eof || rd->offset_raw == rd->current.raw_size)
		return 0;

	if (prefetch_compressed(rd))
		return -1;

	if (size >= (rd->current.raw_size - rd->offset_raw))
		rd->stream->flush(rd->stream);

	ret = rd->stream->read(rd->stream, out, size);
	if (ret < 0)
		return -1;

	rd->offset_raw += ret;
	return ret;
}

int pkg_reader_rewind(pkg_reader_t *rd)
{
	int ret;

	if (lseek(rd->fd, 0, SEEK_SET) == -1) {
		perror(rd->path);
		return -1;
	}

	if (rd->stream != NULL) {
		rd->stream->destroy(rd->stream);
		rd->stream = NULL;
	}

	rd->have_eof = false;
	rd->have_error = false;

	ret = read_header(rd);
	if (ret < 0)
		goto fail;

	if (ret == 0 || rd->current.magic != PKG_MAGIC_HEADER)
		goto fail_header;

	return 0;
fail_header:
	fprintf(stderr, "%s: missing package header\n", rd->path);
fail:
	rd->have_error = true;
	return -1;
}

const char *pkg_reader_get_filename(pkg_reader_t *rd)
{
	return rd->path;
}
