#include <string.h>

#include "compressor.h"

static compressor_t *list = NULL;

void compressor_register(compressor_t *compressor)
{
	compressor->next = list;
	list = compressor;
}

compressor_t *compressor_by_name(const char *name)
{
	compressor_t *it = list;

	while (it != NULL && strcmp(it->name, name) != 0)
		it = it->next;

	return it;
}

compressor_t *compressor_by_id(PKG_COMPRESSION id)
{
	compressor_t *it = list;

	while (it != NULL && it->id != id)
		it = it->next;

	return it;
}
