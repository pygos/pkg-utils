/* SPDX-License-Identifier: ISC */
#include "buildstrategy.h"

static hash_table_t tbl_sourcepkgs;

static int src_pkg_destroy(void *usr, const char *name, void *p)
{
	source_pkg_t *pkg = p;
	(void)name; (void)usr;

	free(pkg->name);
	free(pkg->depends);
	free(pkg);
	return 1;
}

int src_pkg_init(void)
{
	return hash_table_init(&tbl_sourcepkgs, 1024);
}

void src_pkg_cleanup(void)
{
	hash_table_foreach(&tbl_sourcepkgs, NULL, src_pkg_destroy);
	hash_table_cleanup(&tbl_sourcepkgs);
}

source_pkg_t *src_pkg_get(const char *name)
{
	source_pkg_t *src = hash_table_lookup(&tbl_sourcepkgs, name);

	if (src == NULL) {
		src = calloc(1, sizeof(*src));
		if (src == NULL)
			goto fail_oom;

		src->name = strdup(name);
		if (src->name == NULL)
			goto fail_oom;

		if (hash_table_set(&tbl_sourcepkgs, name, src))
			goto fail;
	}
	return src;
fail_oom:
	fputs("out of memory\n", stderr);
fail:
	if (src != NULL)
		free(src->name);
	free(src);
	return NULL;
}

static int remove_untagged(void *usr, const char *name, void *p)
{
	int mask = *((int *)usr);
	source_pkg_t *pkg = p;
	(void)name;

	if ((pkg->flags & mask) == 0) {
		free(pkg->name);
		free(pkg->depends);
		free(pkg);
		return 1;
	}

	return 0;
}

static int find_no_dep(void *usr, const char *name, void *p)
{
	source_pkg_t *pkg = p;
	int *found = usr;

	if (pkg->num_depends == 0) {
		printf("%s\n", name);
		pkg->flags &= ~FLAG_BUILD_PKG;
		*found = 1;
	}

	return 0;
}

static int remove_unmarked_deps(void *usr, const char *name, void *p)
{
	source_pkg_t *pkg = p;
	size_t i = 0;
	(void)usr; (void)name;

	while (i < pkg->num_depends) {
		if ((pkg->depends[i]->flags & FLAG_BUILD_PKG) == 0) {
			pkg->depends[i] = pkg->depends[pkg->num_depends - 1];
			pkg->num_depends -= 1;
		} else {
			++i;
		}
	}

	return 0;
}

int src_pkg_output_build_order(void)
{
	int i = FLAG_BUILD_PKG;

	hash_table_foreach(&tbl_sourcepkgs, &i, remove_untagged);

	while (tbl_sourcepkgs.count > 0) {
		i = 0;
		hash_table_foreach(&tbl_sourcepkgs, &i, find_no_dep);
		if (!i) {
			fputs("cycle detected in package dependencies!\n",
			      stderr);
			return -1;
		}

		hash_table_foreach(&tbl_sourcepkgs, NULL, remove_unmarked_deps);

		i = FLAG_BUILD_PKG;
		hash_table_foreach(&tbl_sourcepkgs, &i, remove_untagged);
	}

	return 0;
}
