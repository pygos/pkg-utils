#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <lzma.h>

#include "compressor.h"

#define CHUNK_SIZE 16384

typedef struct {
	compressor_stream_t base;
	lzma_stream strm;
	lzma_action action;
	uint8_t chunk[CHUNK_SIZE];
	int used;
	bool eof;
	bool error;
} lzma_stream_t;

static ssize_t lzma_write(compressor_stream_t *base,
			  const uint8_t *in, size_t size)
{
	lzma_stream_t *lzma = (lzma_stream_t *)base;

	if (size > (size_t)(CHUNK_SIZE - lzma->used))
		size = CHUNK_SIZE - lzma->used;

	if (size == 0)
		return 0;

	memcpy(lzma->chunk + lzma->used, in, size);
	lzma->used += size;
	return size;
}

static ssize_t lzma_read(compressor_stream_t *base,
			 uint8_t *out, size_t size)
{
	lzma_stream_t *lzma = (lzma_stream_t *)base;
	int have, total = 0, processed;

	if (lzma->error)
		return -1;
	if (lzma->eof)
		return 0;

	lzma->strm.avail_in = lzma->used;
	lzma->strm.next_in = lzma->chunk;

	do {
		lzma->strm.avail_out = size;
		lzma->strm.next_out = out;

		switch (lzma_code(&lzma->strm, lzma->action)) {
		case LZMA_STREAM_END:
			lzma->eof = true;
			break;
		case LZMA_BUF_ERROR:
			total += (size - lzma->strm.avail_out);
			goto out_remove;
		case LZMA_OK:
			break;
		default:
			lzma->error = true;
			return -1;
		}

		have = size - lzma->strm.avail_out;

		out += have;
		size -= have;
		total += have;
	} while (lzma->strm.avail_out == 0 && !lzma->eof);
out_remove:
	processed = lzma->used - lzma->strm.avail_in;

	if (processed < lzma->used) {
		memmove(lzma->chunk, lzma->chunk + processed,
			lzma->used - processed);
	}

	lzma->used -= processed;
	return total;
}

static void lzma_flush(compressor_stream_t *base)
{
	lzma_stream_t *lzma = (lzma_stream_t *)base;

	lzma->action = LZMA_FINISH;
}

static void lzma_destroy(compressor_stream_t *base)
{
	lzma_stream_t *lzma = (lzma_stream_t *)base;

	lzma_end(&lzma->strm);
	free(lzma);
}

static compressor_stream_t *create_stream(bool compress)
{
	lzma_stream_t *lzma = calloc(1, sizeof(*lzma));
	compressor_stream_t *base;
	lzma_filter filters[5];
	int ret;

	if (lzma == NULL) {
		perror("creating lzma stream");
		return NULL;
	}

	lzma->action = LZMA_RUN;

	base = (compressor_stream_t *)lzma;
	base->write = lzma_write;
	base->read = lzma_read;
	base->flush = lzma_flush;
	base->destroy = lzma_destroy;

	if (compress) {
		lzma_options_lzma opt_lzma2;

		if (lzma_lzma_preset(&opt_lzma2, LZMA_PRESET_DEFAULT))
			goto fail;

		filters[0].id = LZMA_FILTER_LZMA2;
		filters[0].options = &opt_lzma2;

		filters[1].id = LZMA_VLI_UNKNOWN;
		filters[1].options = NULL;

		ret = lzma_stream_encoder(&lzma->strm, filters,
					  LZMA_CHECK_CRC32);
	} else {
		ret = lzma_stream_decoder(&lzma->strm, UINT64_MAX, 0);
	}

	if (ret != LZMA_OK)
		goto fail;

	return base;
fail:
	fputs("internal error creating lzma stream\n", stderr);
	free(lzma);
	return NULL;
}

static compressor_stream_t *lzma_compress(compressor_t *cmp)
{
	(void)cmp;
	return create_stream(true);
}

static compressor_stream_t *lzma_uncompress(compressor_t *cmp)
{
	(void)cmp;
	return create_stream(false);
}

static compressor_t lzma = {
	.name = "lzma",
	.id = PKG_COMPRESSION_LZMA,
	.compression_stream = lzma_compress,
	.uncompression_stream = lzma_uncompress,
};

REGISTER_COMPRESSOR(lzma)
