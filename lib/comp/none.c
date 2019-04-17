/* SPDX-License-Identifier: ISC */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zlib.h>

#include "internal.h"

#define CHUNK_SIZE 16384

typedef struct {
	compressor_stream_t base;
	uint8_t chunk[CHUNK_SIZE];
	size_t used;
} dummy_stream_t;

static ssize_t dummy_write(compressor_stream_t *base,
			   const uint8_t *in, size_t size)
{
	dummy_stream_t *dummy = (dummy_stream_t *)base;

	if (size > (CHUNK_SIZE - dummy->used))
		size = CHUNK_SIZE - dummy->used;

	if (size == 0)
		return 0;

	memcpy(dummy->chunk + dummy->used, in, size);
	dummy->used += size;
	return size;
}

static ssize_t dummy_read(compressor_stream_t *base,
			  uint8_t *out, size_t size)
{
	dummy_stream_t *dummy = (dummy_stream_t *)base;

	if (size > dummy->used)
		size = dummy->used;

	if (size == 0)
		return 0;

	memcpy(out, dummy->chunk, size);

	if (size < dummy->used) {
		memmove(dummy->chunk, dummy->chunk + size,
			dummy->used - size);
	}

	dummy->used -= size;
	return size;
}

static ssize_t dummy_do_block(compressor_stream_t *base,
			      const uint8_t *in, uint8_t *out,
			      size_t insize, size_t outsize)
{
	(void)base;

	if (outsize < insize)
		return 0;

	memcpy(out, in, insize);
	return insize;
}

static void dummy_flush(compressor_stream_t *base)
{
	(void)base;
}

static void dummy_destroy(compressor_stream_t *base)
{
	free(base);
}

static compressor_stream_t *create_dummy_stream(compressor_t *cmp,
						void *options)
{
	dummy_stream_t *dummy = calloc(1, sizeof(*dummy));
	compressor_stream_t *base;
	(void)cmp; (void)options;

	if (dummy == NULL) {
		perror("creating dummy compressor stream");
		return NULL;
	}

	base = (compressor_stream_t *)dummy;
	base->write = dummy_write;
	base->read = dummy_read;
	base->flush = dummy_flush;
	base->do_block = dummy_do_block;
	base->destroy = dummy_destroy;
	return base;
}

static compressor_stream_t *create_dummy_uncompressor(compressor_t *cmp)
{
	return create_dummy_stream(cmp, NULL);
}

compressor_t comp_none = {
	.name = "none",
	.id = PKG_COMPRESSION_NONE,
	.compression_stream = create_dummy_stream,
	.uncompression_stream = create_dummy_uncompressor,
};
