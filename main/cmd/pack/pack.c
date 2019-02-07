#include "pack.h"

static const struct option long_opts[] = {
	{ "toc-compressor", required_argument, NULL, 't' },
	{ "file-compressor", required_argument, NULL, 'f' },
	{ "description", required_argument, NULL, 'd' },
	{ "file-list", required_argument, NULL, 'l' },
	{ "repo-dir", required_argument, NULL, 'r' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "t:f:l:r:d:";

static compressor_t *get_default_compressor(void)
{
	compressor_t *cmp;

	cmp = compressor_by_id(PKG_COMPRESSION_LZMA);
	if (cmp != NULL)
		return cmp;

	cmp = compressor_by_id(PKG_COMPRESSION_ZLIB);
	if (cmp != NULL)
		return cmp;

	cmp = compressor_by_id(PKG_COMPRESSION_NONE);
	if (cmp != NULL)
		return cmp;

	return cmp;
}

static compressor_t *try_get_compressor(const char *name)
{
	compressor_t *cmp = compressor_by_name(name);

	if (cmp == NULL)
		fprintf(stderr, "unkown compressor: %s\n", name);

	return cmp;
}

static pkg_writer_t *open_writer(pkg_desc_t *desc, const char *repodir)
{
	char *path;

	if (mkdir_p(repodir))
		return NULL;

	path = alloca(strlen(repodir) + strlen(desc->name) + 16);
	sprintf(path, "%s/%s.pkg", repodir, desc->name);

	return pkg_writer_open(path);
}

static int cmd_pack(int argc, char **argv)
{
	const char *filelist = NULL, *repodir = NULL, *descfile = NULL;
	compressor_t *cmp_toc, *cmp_files;
	image_entry_t *list = NULL;
	pkg_writer_t *wr;
	pkg_desc_t desc;
	int i;

	cmp_toc = get_default_compressor();
	cmp_files = cmp_toc;

	if (cmp_toc == NULL) {
		fputs("no compressor implementations available\n", stderr);
		return EXIT_FAILURE;
	}

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 't':
			cmp_toc = try_get_compressor(optarg);
			if (cmp_toc == NULL)
				return EXIT_FAILURE;
			break;
		case 'f':
			cmp_files = try_get_compressor(optarg);
			if (cmp_files == NULL)
				return EXIT_FAILURE;
			break;
		case 'l':
			filelist = optarg;
			break;
		case 'd':
			descfile = optarg;
			break;
		case 'r':
			repodir = optarg;
			break;
		default:
			tell_read_help(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (descfile == NULL) {
		fputs("missing argument: package description file\n", stderr);
		tell_read_help(argv[0]);
		return EXIT_FAILURE;
	}

	if (repodir == NULL) {
		fputs("missing argument: repository directory\n", stderr);
		tell_read_help(argv[0]);
		return EXIT_FAILURE;
	}

	if (optind < argc)
		fputs("warning: ignoring extra arguments\n", stderr);

	if (desc_read(descfile, &desc))
		return EXIT_FAILURE;

	if (filelist != NULL && filelist_read(filelist, &list))
		goto fail_desc;

	wr = open_writer(&desc, repodir);
	if (wr == NULL)
		goto fail_fp;

	if (write_header_data(wr, &desc))
		goto fail;

	if (list != NULL) {
		if (write_toc(wr, list, cmp_toc))
			goto fail;

		if (write_files(wr, list, cmp_files))
			goto fail;
	}

	pkg_writer_close(wr);
	image_entry_free_list(list);
	desc_free(&desc);
	return EXIT_SUCCESS;
fail:
	pkg_writer_close(wr);
fail_fp:
	image_entry_free_list(list);
fail_desc:
	desc_free(&desc);
	return EXIT_FAILURE;
}

static command_t pack = {
	.cmd = "pack",
	.usage = "OPTIONS...",
	.s_desc = "generate a package file",
	.l_desc =
"Read a list of files from the given lists file and generate a package.\n"
"Possible options:\n"
"  --file-list, -l <path>   Specify a file containing a list of input files.\n"
"  --repo-dir, -r <path>    Specify the output repository path to store the\n"
"                           package in.\n"
"  --description, -d <path> Specify a file containing a description of the\n"
"                           package, including information such as package\n"
"                           dependencies, the actual package name, etc.\n"
"\n"
"  --toc-compressor, -t <compressor>\n"
"  --file-compressor, -f <compressor>\n"
"      Specify what compressor to use for compressing the table of contents,\n"
"      or for compressing files respectively.\n",
	.run_cmd = cmd_pack,
};

REGISTER_COMMAND(pack)
