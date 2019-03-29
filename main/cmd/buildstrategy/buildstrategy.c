/* SPDX-License-Identifier: ISC */
#include "buildstrategy.h"

typedef int (*line_handler_t)(const char *filename, size_t linenum,
			      const char *lhs, const char *rhs);

struct userdata {
	line_handler_t cb;
};

static int handle_line(void *usr, const char *filename,
		       size_t linenum, char *line)
{
	struct userdata *u = usr;
	char *rhs;

	rhs = strchr(line, ',');
	if (rhs == NULL)
		return 0;

	*(rhs++) = '\0';
	while (isspace(*rhs))
		++rhs;

	if (*line == '\0' || *rhs == '\0')
		return 0;

	return u->cb(filename, linenum, line, rhs);
}

static int foreach_line(const char *filename, line_handler_t cb)
{
	struct userdata u = { cb };

	return foreach_line_in_file(filename, &u, handle_line);
}

/*****************************************************************************/

static hash_table_t tbl_preferes;
static hash_table_t tbl_provides;
static hash_table_t tbl_sourcepkgs;

static source_pkg_t *get_src_pkg(const char *name)
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

static source_pkg_t *get_provider(const char *parent, const char *binpkg)
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

static int handle_depends(const char *filename, size_t linenum,
			  const char *sourcepkg, const char *binpkg)
{
	source_pkg_t *src, *dep;
	size_t count;
	void *new;
	(void)filename; (void)linenum;

	src = hash_table_lookup(&tbl_sourcepkgs, sourcepkg);
	if (src == NULL)
		return 0;

	dep = get_provider(sourcepkg, binpkg);
	if (dep == NULL)
		return -1;

	if ((src->num_depends % 10) == 0) {
		count = src->num_depends + 10;

		new = realloc(src->depends, sizeof(src->depends[0]) * count);
		if (new == NULL) {
			fputs("out of memory\n", stderr);
			return -1;
		}

		src->depends = new;
	}

	src->depends[src->num_depends++] = dep;
	return 0;
}

static int handle_provides(const char *filename, size_t linenum,
			   const char *sourcepkg, const char *binpkg)
{
	source_pkg_t *head = hash_table_lookup(&tbl_provides, binpkg), *src;
	(void)filename; (void)linenum;

	if (head != NULL && strcmp(head->name, sourcepkg) == 0)
		return 0;

	src = get_src_pkg(sourcepkg);
	if (src == NULL)
		return -1;

	src->next = head;
	return hash_table_set(&tbl_provides, binpkg, src);
}

static int handle_prefere(const char *filename, size_t linenum,
			  const char *binpkg, const char *sourcepkg)
{
	source_pkg_t *src = hash_table_lookup(&tbl_preferes, binpkg);

	if (src != NULL) {
		fprintf(stderr,
			"%s: %zu: preferred provider for %s already "
			"set to %s\n", filename, linenum, binpkg, src->name);
		return -1;
	}

	src = get_src_pkg(sourcepkg);
	if (src == NULL)
		return -1;

	return hash_table_set(&tbl_preferes, binpkg, src);
}

/*****************************************************************************/

static const struct option long_opts[] = {
	{ "prefere", required_argument, NULL, 'P' },
	{ "provides", required_argument, NULL, 'p' },
	{ "depends", required_argument, NULL, 'd' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "p:d:P:";

static int process_args(int argc, char **argv)
{
	const char *provides = NULL, *depends = NULL, *prefere = NULL;
	int i;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'P':
			prefere = optarg;
			break;
		case 'p':
			provides = optarg;
			break;
		case 'd':
			depends = optarg;
			break;
		default:
			goto fail_arg;
		}
	}

	if (optind >= argc) {
		fputs("no packages specified\n", stderr);
		goto fail_arg;
	}

	if (provides == NULL) {
		fputs("no source package provides specified\n", stderr);
		goto fail_arg;
	}

	if (hash_table_init(&tbl_provides, 1024))
		return -1;

	if (hash_table_init(&tbl_sourcepkgs, 1024))
		goto fail_provides;

	if (hash_table_init(&tbl_preferes, 1024))
		goto fail_srcpkg;

	if (prefere != NULL && foreach_line(prefere, handle_prefere) != 0)
		goto fail_prefere;

	if (foreach_line(provides, handle_provides))
		goto fail_prefere;

	if (depends != NULL && foreach_line(depends, handle_depends) != 0)
		goto fail_prefere;

	return 0;
fail_prefere:
	hash_table_cleanup(&tbl_preferes);
fail_srcpkg:
	hash_table_cleanup(&tbl_sourcepkgs);
fail_provides:
	hash_table_cleanup(&tbl_provides);
	return -1;
fail_arg:
	tell_read_help(argv[0]);
	return -1;
}

static void pkg_mark_deps(source_pkg_t *pkg)
{
	size_t i;

	if (pkg->flags & FLAG_BUILD_PKG)
		return;

	pkg->flags |= FLAG_BUILD_PKG;

	for (i = 0; i < pkg->num_depends; ++i) {
		if ((pkg->depends[i]->flags & FLAG_BUILD_PKG) == 0)
			pkg_mark_deps(pkg->depends[i]);
	}
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

static int cmd_buildstrategy(int argc, char **argv)
{
	int i, ret = EXIT_FAILURE;
	source_pkg_t *pkg;

	if (process_args(argc, argv))
		return EXIT_FAILURE;

	for (i = optind; i < argc; ++i) {
		pkg = get_provider(NULL, argv[i]);
		if (pkg == NULL)
			goto out;

		pkg_mark_deps(pkg);
	}

	i = FLAG_BUILD_PKG;
	hash_table_foreach(&tbl_sourcepkgs, &i, remove_untagged);

	while (tbl_sourcepkgs.count > 0) {
		i = 0;
		hash_table_foreach(&tbl_sourcepkgs, &i, find_no_dep);
		if (!i) {
			fputs("cycle detected in package dependencies!\n",
			      stderr);
			goto out;
		}

		hash_table_foreach(&tbl_sourcepkgs, NULL, remove_unmarked_deps);

		i = FLAG_BUILD_PKG;
		hash_table_foreach(&tbl_sourcepkgs, &i, remove_untagged);
	}

	ret = EXIT_SUCCESS;
out:
	i = 0;
	hash_table_foreach(&tbl_sourcepkgs, &i, remove_untagged);
	hash_table_cleanup(&tbl_sourcepkgs);
	hash_table_cleanup(&tbl_provides);
	hash_table_cleanup(&tbl_preferes);
	return ret;
}

static command_t buildstrategy = {
	.cmd = "buildstrategy",
	.usage = "[OPTIONS...] <packages>",
	.s_desc = "work out what source packages to build in what order",
	.l_desc =
"The buildstrategy command takes as input a description of what binary\n"
"packages a source package needs in order to build and what binary packages\n"
"in turn produces in the process. The command then takes from the command\n"
"line arguments a list of desired binary packages and works out a minimal\n"
"set of source packages to build and in what order they should be built.\n"
"\n"
"Possible options:\n"
"  --provides, -p <file>  A two column CSV file. Each line contains the name\n"
"                         of a source package and a binary package that it\n"
"                         produces when built. If a source packages provides\n"
"                         multiple binary packages, use one line for each\n"
"                         binary package.\n"
"  --depends, -d <file>   A two column CSV file. Each line contains the name\n"
"                         of a source package and a binary package that it\n"
"                         requires to build.\n"
"  --prefere, -P <file>   A two column CSV file. Each line contains the name\n"
"                         of a binary package and a source package that\n"
"                         produces this binary. If the `--provides` file\n"
"                         specifies that more than one source pacakge\n"
"                         provides a binary package, this file contains the\n"
"                         prefered one we should use.\n",
	.run_cmd = cmd_buildstrategy,
};

REGISTER_COMMAND(buildstrategy)
