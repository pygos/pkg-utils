/* SPDX-License-Identifier: ISC */
#include "buildstrategy.h"

static hash_table_t tbl_provides;
static hash_table_t tbl_preferes;

int provider_init(void)
{
	if (hash_table_init(&tbl_provides, 1024))
		return -1;

	if (hash_table_init(&tbl_preferes, 1024)) {
		hash_table_cleanup(&tbl_provides);
		return -1;
	}

	return 0;
}

void provider_cleanup(void)
{
	hash_table_cleanup(&tbl_provides);
	hash_table_cleanup(&tbl_preferes);
}

int provider_add(const char *sourcepkg, const char *binpkg)
{
	source_pkg_t *head = hash_table_lookup(&tbl_provides, binpkg), *src;

	for (src = head; src != NULL; src = src->next) {
		if (strcmp(src->name, sourcepkg) == 0)
			return 0;
	}

	src = src_pkg_get(sourcepkg);
	if (src == NULL)
		return -1;

	src->next = head;
	return hash_table_set(&tbl_provides, binpkg, src);
}

int provider_add_prefered(const char *binpkg, const char *sourcepkg)
{
	source_pkg_t *src = src_pkg_get(sourcepkg);

	if (src == NULL)
		return -1;

	return hash_table_set(&tbl_preferes, binpkg, src);
}

source_pkg_t *provider_get(const char *parent, const char *binpkg)
{
	source_pkg_t *spkg, *pref;

	spkg = hash_table_lookup(&tbl_provides, binpkg);
	if (spkg == NULL)
		goto fail_none;

	pref = hash_table_lookup(&tbl_preferes, binpkg);

	if (spkg->next != NULL) {
		if (pref == NULL)
			goto fail_no_pref;

		while (spkg != NULL && strcmp(spkg->name, pref->name) != 0) {
			spkg = spkg->next;
		}

		if (spkg == NULL) {
			spkg = hash_table_lookup(&tbl_provides, binpkg);
			goto fail_provider;
		}
	} else {
		if (pref != NULL && strcmp(spkg->name, pref->name) != 0)
			goto fail_provider;
	}

	return spkg;
fail_none:
	fprintf(stderr, "No source package provides binary package '%s'.\n\n",
		binpkg);
	goto fail;
fail_provider:
	fprintf(stderr,
		"Preferred provider for binary package '%s' is set to\n"
		"source package '%s', which does not provide '%s'.\n\n",
		binpkg, pref->name, binpkg);
	goto fail_list_providers;
fail_no_pref:
	fprintf(stderr, "No preferred provider set for "
		"binary package '%s'.\n\n", binpkg);
	goto fail_list_providers;
fail_list_providers:
	fprintf(stderr, "The following source packages provide the "
		"binary package '%s':\n\n", binpkg);
	while (spkg != NULL) {
		fprintf(stderr, "\t%s\n", spkg->name);
		spkg = spkg->next;
	}
	fputc('\n', stderr);
	goto fail;
fail:
	if (parent != NULL) {
		fprintf(stderr, "Binary package '%s' is required to "
			"build source package '%s'\n", binpkg, parent);
	}
	return NULL;
}
