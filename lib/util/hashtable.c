/* SPDX-License-Identifier: ISC */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "util/hashtable.h"

/* R5 hash function (borrowed from reiserfs) */
static uint32_t strhash(const char *s)
{
	const signed char *str = (const signed char *)s;
	uint32_t a = 0;

	while (*str != '\0') {
		a += *str << 4;
		a += *str >> 4;
		a *= 11;
		str++;
	}

	return a;
}

int hash_table_init(hash_table_t *table, size_t size)
{
	table->num_buckets = size;
	table->count = 0;
	table->buckets = calloc(size, sizeof(table->buckets[0]));

	if (table->buckets == NULL) {
		fputs("out of memory\n", stderr);
		return -1;
	}

	return 0;
}

void hash_table_cleanup(hash_table_t *table)
{
	hash_bucket_t *bucket;
	size_t i;

	for (i = 0; i < table->num_buckets; ++i) {
		while (table->buckets[i] != NULL) {
			bucket = table->buckets[i];
			table->buckets[i] = bucket->next;

			free(bucket->key);
			free(bucket);
		}
	}

	free(table->buckets);

	table->buckets = NULL;
	table->num_buckets = 0;
	table->count = 0;
}

void *hash_table_lookup(hash_table_t *table, const char *key)
{
	hash_bucket_t *bucket;
	uint32_t hash;

	hash = strhash(key);
	bucket = table->buckets[hash % table->num_buckets];

	while (bucket != NULL) {
		if (strcmp(bucket->key, key) == 0)
			return bucket->value;

		bucket = bucket->next;
	}

	return NULL;
}

int hash_table_set(hash_table_t *table, const char *key, void *value)
{
	hash_bucket_t *bucket;
	uint32_t hash;
	size_t index;

	hash = strhash(key);
	index = hash % table->num_buckets;
	bucket = table->buckets[index];

	while (bucket != NULL) {
		if (strcmp(bucket->key, key) == 0) {
			bucket->value = value;
			return 0;
		}

		bucket = bucket->next;
	}

	bucket = calloc(1, sizeof(*bucket));
	if (bucket == NULL)
		goto fail_oom;

	bucket->key = strdup(key);
	if (bucket->key == NULL)
		goto fail_oom;

	bucket->value = value;
	bucket->next = table->buckets[index];

	table->buckets[index] = bucket;
	table->count += 1;
	return 0;
fail_oom:
	free(bucket);
	fputs("out of memory\n", stderr);
	return -1;
}

void hash_table_foreach(hash_table_t *table, void *usr,
			int(*fun)(void *usr, const char *key, void *value))
{
	hash_bucket_t *bucket, *prev;
	size_t i;

	for (i = 0; i < table->num_buckets; ++i) {
		prev = NULL;
		bucket = table->buckets[i];

		while (bucket != NULL) {
			if (fun(usr, bucket->key, bucket->value)) {
				if (prev == NULL) {
					table->buckets[i] = bucket->next;
					free(bucket->key);
					free(bucket);
					bucket = table->buckets[i];
				} else {
					prev->next = bucket->next;
					free(bucket->key);
					free(bucket);
					bucket = prev->next;
				}
				table->count -= 1;
			} else {
				prev = bucket;
				bucket = bucket->next;
			}
		}
	}
}
