/* SPDX-License-Identifier: ISC */
#ifndef HASH_TABLE_H
#define HASH_TABLE_H

typedef struct hash_bucket_t {
	struct hash_bucket_t *next;
	char *key;
	void *value;
} hash_bucket_t;

typedef struct {
	hash_bucket_t **buckets;
	size_t num_buckets;
	size_t count;
} hash_table_t;

int hash_table_init(hash_table_t *table, size_t size);

void hash_table_cleanup(hash_table_t *table);

void *hash_table_lookup(hash_table_t *table, const char *key);

int hash_table_set(hash_table_t *table, const char *key, void *value);

void hash_table_foreach(hash_table_t *table, void *usr,
			int(*fun)(void *usr, const char *key, void *value));

#endif /* HASH_TABLE_H */
