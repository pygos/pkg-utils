#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zlib.h>

#include "compressor.h"

#define CHUNK_SIZE 16384

typedef struct {
	compressor_stream_t base;
	z_stream strm;
	uint8_t chunk[CHUNK_SIZE];
	int used;
	int flush_mode;
	bool compress;
	bool eof;
	bool error;
} zlib_stream_t;

static ssize_t zlib_write(compressor_stream_t *base,
			  const uint8_t *in, size_t size)
{
	zlib_stream_t *zlib = (zlib_stream_t *)base;

	if (size > (size_t)(CHUNK_SIZE - zlib->used))
		size = CHUNK_SIZE - zlib->used;

	if (size == 0)
		return 0;

	memcpy(zlib->chunk + zlib->used, in, size);
	zlib->used += size;
	return size;
}

static ssize_t zlib_read(compressor_stream_t *base,
			 uint8_t *out, size_t size)
{
	zlib_stream_t *zlib = (zlib_stream_t *)base;
	int ret, have, total = 0, processed;

	if (zlib->error)
		return -1;
	if (zlib->eof)
		return 0;

	zlib->strm.avail_in = zlib->used;
	zlib->strm.next_in = zlib->chunk;

	do {
		zlib->strm.avail_out = size;
		zlib->strm.next_out = out;

		if (zlib->compress) {
			ret = deflate(&zlib->strm, zlib->flush_mode);
		} else {
			ret = inflate(&zlib->strm, zlib->flush_mode);
		}

		switch (ret) {
		case Z_STREAM_END:
			zlib->eof = true;
			break;
		case Z_BUF_ERROR:
			total += (size - zlib->strm.avail_out);
			goto out_remove;
		case Z_OK:
			break;
		default:
			zlib->error = true;
			return -1;
		}

		have = size - zlib->strm.avail_out;

		out += have;
		size -= have;
		total += have;
	} while (zlib->strm.avail_out == 0 && !zlib->eof);
out_remove:
	processed = zlib->used - zlib->strm.avail_in;

	if (processed < zlib->used) {
		memmove(zlib->chunk, zlib->chunk + processed,
			zlib->used - processed);
	}

	zlib->used -= processed;
	return total;
}

static void zlib_flush(compressor_stream_t *base)
{
	zlib_stream_t *zlib = (zlib_stream_t *)base;

	zlib->flush_mode = Z_FINISH;
}

static void zlib_destroy(compressor_stream_t *base)
{
	zlib_stream_t *zlib = (zlib_stream_t *)base;

	if (zlib->compress) {
		deflateEnd(&zlib->strm);
	} else {
		inflateEnd(&zlib->strm);
	}

	free(zlib);
}

static compressor_stream_t *create_stream(bool compress)
{
	zlib_stream_t *zlib = calloc(1, sizeof(*zlib));
	compressor_stream_t *base;
	int ret;

	if (zlib == NULL) {
		perror("creating zlib stream");
		return NULL;
	}

	zlib->flush_mode = Z_NO_FLUSH;
	zlib->compress = compress;

	base = (compressor_stream_t *)zlib;
	base->write = zlib_write;
	base->read = zlib_read;
	base->flush = zlib_flush;
	base->destroy = zlib_destroy;

	if (compress) {
		ret = deflateInit(&zlib->strm, Z_BEST_COMPRESSION);
	} else {
		ret = inflateInit(&zlib->strm);
	}

	if (ret != Z_OK) {
		fputs("internal error creating zlib stream\n", stderr);
		free(zlib);
		return NULL;
	}

	return base;
}

static compressor_stream_t *zlib_compress(compressor_t *cmp)
{
	(void)cmp;
	return create_stream(true);
}

static compressor_stream_t *zlib_uncompress(compressor_t *cmp)
{
	(void)cmp;
	return create_stream(false);
}

static compressor_t zlib = {
	.name = "zlib",
	.id = PKG_COMPRESSION_ZLIB,
	.compression_stream = zlib_compress,
	.uncompression_stream = zlib_uncompress,
};

REGISTER_COMPRESSOR(zlib)
