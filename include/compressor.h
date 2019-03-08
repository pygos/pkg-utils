/* SPDX-License-Identifier: ISC */
#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <stddef.h>
#include <sys/types.h>

#include "pkgformat.h"

typedef struct compressor_stream_t {
	ssize_t (*write)(struct compressor_stream_t *stream, const uint8_t *in,
			 size_t size);

	ssize_t (*read)(struct compressor_stream_t *stream, uint8_t *out,
			size_t size);

	void (*flush)(struct compressor_stream_t *stream);

	void (*destroy)(struct compressor_stream_t *stream);
} compressor_stream_t;

typedef struct compressor_t {
	struct compressor_t *next;
	const char *name;
	PKG_COMPRESSION id;

	compressor_stream_t *(*compression_stream)(struct compressor_t *cmp);

	compressor_stream_t *(*uncompression_stream)(struct compressor_t *cmp);
} compressor_t;

void compressor_register(compressor_t *compressor);

compressor_t *compressor_by_name(const char *name);

compressor_t *compressor_by_id(PKG_COMPRESSION id);

#define REGISTER_COMPRESSOR(compressor) \
	static void __attribute__((constructor)) register_##compressor(void) \
	{ \
		compressor_register((compressor_t *)&compressor); \
	}

#endif /* COMPRESSOR_H */
