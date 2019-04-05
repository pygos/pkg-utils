/* SPDX-License-Identifier: ISC */
#include <string.h>

#include "internal.h"

static compressor_t *compressors[] = {
	[PKG_COMPRESSION_NONE] = &comp_none,
#ifdef WITH_ZLIB
	[PKG_COMPRESSION_ZLIB] = &comp_zlib,
#endif
#ifdef WITH_LZMA
	[PKG_COMPRESSION_LZMA] = &comp_lzma,
#endif
};

compressor_t *compressor_by_name(const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(compressors) / sizeof(compressors[0]); ++i) {
		if (compressors[i] == NULL)
			continue;
		if (strcmp(compressors[i]->name, name) == 0)
			return compressors[i];
	}

	return NULL;
}

compressor_t *compressor_by_id(PKG_COMPRESSION id)
{
	if ((int)id < 0)
		return NULL;

	if ((size_t)id >= sizeof(compressors) / sizeof(compressors[0]))
		return NULL;

	return compressors[id];
}
